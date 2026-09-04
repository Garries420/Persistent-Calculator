// Licensed under the MIT License.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;

using CalculatorApp.JsonUtils;
using CalculatorApp.ViewModel.Snapshot;

using Windows.Storage;

namespace CalculatorApp.Common
{
    public sealed class PersistentHistoryEntry
    {
        [JsonPropertyName("id")]
        public string Id { get; set; }

        [JsonPropertyName("sessionKey")]
        public string SessionKey { get; set; }

        [JsonPropertyName("mode")]
        public string Mode { get; set; }

        [JsonPropertyName("expression")]
        public string Expression { get; set; }

        [JsonPropertyName("result")]
        public string Result { get; set; }

        [JsonPropertyName("sourceValue")]
        public string SourceValue { get; set; }

        [JsonPropertyName("sourceUnit")]
        public string SourceUnit { get; set; }

        [JsonPropertyName("targetUnit")]
        public string TargetUnit { get; set; }

        [JsonPropertyName("calculatorValue")]
        public string CalculatorValue { get; set; }

        [JsonPropertyName("programmerRadix")]
        public string ProgrammerRadix { get; set; }

        [JsonPropertyName("programmerBitLength")]
        public string ProgrammerBitLength { get; set; }

        [JsonPropertyName("scientificAngle")]
        public string ScientificAngle { get; set; }

        [JsonPropertyName("scientificFToE")]
        public bool? ScientificFToE { get; set; }

        [JsonPropertyName("timestampUtc")]
        public DateTimeOffset? TimestampUtc { get; set; }

        [JsonIgnore]
        public string TimestampLabel => TimestampUtc.HasValue
            ? TimestampUtc.Value.ToLocalTime().ToString("yyyy-MM-dd \u00B7 HH:mm", CultureInfo.CurrentCulture)
            : "Earlier";

        internal PersistentHistoryEntry Clone()
        {
            return (PersistentHistoryEntry)MemberwiseClone();
        }
    }

    internal sealed class PersistentHistoryService
    {
        private const int CurrentFormatVersion = 3;
        private const int MaximumEntries = 500;
        private const ulong MaximumHistoryFileBytes = 8 * 1024 * 1024;

        private readonly SemaphoreSlim _fileLock = new SemaphoreSlim(1, 1);
        private readonly List<PersistentHistoryEntry> _entries = new List<PersistentHistoryEntry>();
        private readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions
        {
            AllowTrailingCommas = false,
            PropertyNameCaseInsensitive = false,
            ReadCommentHandling = JsonCommentHandling.Disallow,
            WriteIndented = true
        };

        private CalcManagerSnapshot _latestSnapshot;

        public IReadOnlyList<PersistentHistoryEntry> GetEntries()
        {
            return _entries
                .Select((entry, index) => new { Entry = entry, Index = index })
                .OrderByDescending(item => item.Entry.TimestampUtc ?? DateTimeOffset.MinValue)
                .ThenByDescending(item => item.Index)
                .Select(item => item.Entry.Clone())
                .ToList();
        }

        public async Task<CalcManagerSnapshot> LoadAsync()
        {
            await _fileLock.WaitAsync();
            try
            {
                var folder = await PersistentDataStore.GetFolderAsync();
                var file = await TryGetFileAsync(folder, PersistentDataStore.HistoryFileName);
                PersistentHistoryDocument document = null;

                if (file != null)
                {
                    try
                    {
                        var properties = await file.GetBasicPropertiesAsync();
                        if (properties.Size > 0 && properties.Size <= MaximumHistoryFileBytes)
                        {
                            var json = await FileIO.ReadTextAsync(file);
                            document = JsonSerializer.Deserialize<PersistentHistoryDocument>(json, _jsonOptions);
                        }
                    }
                    catch (Exception ex)
                    {
                        // A damaged current file must not prevent legacy migration or
                        // cleanup from running. Keep the bad file untouched until the
                        // first successful atomic write replaces it.
                        ViewModel.Common.TraceLogger.GetInstance().LogRecallError(
                            $"Existing history document could not be parsed. {ex.GetType().Name}: {ex.Message}");
                    }
                }

                if (document != null && (document.FormatVersion == 2 || document.FormatVersion == CurrentFormatVersion))
                {
                    _latestSnapshot = document.HistoryState?.Value;
                    ImportDocumentEntries(document);
                }

                // The old C build stored these two files directly in Documents.
                // Import history first, write it atomically, then remove the source.
                // Its obsolete rates cache is never copied.
                await PersistentDataStore.DeleteLegacyCurrencyRatesFileAsync();
                var legacyFile = await PersistentDataStore.TryGetLegacyHistoryFileAsync();
                if (legacyFile != null)
                {
                    var properties = await legacyFile.GetBasicPropertiesAsync();
                    if (properties.Size <= MaximumHistoryFileBytes)
                    {
                        var legacyText = await FileIO.ReadTextAsync(legacyFile);
                        MergeLegacyEntries(ParseLegacyHistory(legacyText));
                        await WriteDocumentLockedAsync();
                        await PersistentDataStore.DeleteLegacyHistoryFileAsync();
                    }
                }

                return _latestSnapshot;
            }
            catch (Exception ex)
            {
                ViewModel.Common.TraceLogger.GetInstance().LogRecallError(
                    $"Persistent history could not be loaded. {ex.GetType().Name}: {ex.Message}");
                return _latestSnapshot;
            }
            finally
            {
                _fileLock.Release();
            }
        }

        public async Task SaveAsync(CalcManagerSnapshot snapshot)
        {
            await _fileLock.WaitAsync();
            try
            {
                _latestSnapshot = snapshot;
                await WriteDocumentLockedAsync();
            }
            catch (Exception ex)
            {
                LogSaveError(ex);
            }
            finally
            {
                _fileLock.Release();
            }
        }

        public async Task<ulong?> GetHistoryFileSizeAsync()
        {
            await _fileLock.WaitAsync();
            try
            {
                var folder = await PersistentDataStore.GetFolderAsync();
                var file = await TryGetFileAsync(folder, PersistentDataStore.HistoryFileName);
                if (file == null)
                {
                    return 0;
                }

                var properties = await file.GetBasicPropertiesAsync();
                return properties.Size;
            }
            catch (Exception ex)
            {
                ViewModel.Common.TraceLogger.GetInstance().LogRecallError(
                    $"Persistent history size could not be read. {ex.GetType().Name}: {ex.Message}");
                return null;
            }
            finally
            {
                _fileLock.Release();
            }
        }

        public async Task RecordSessionAsync(
            string sessionKey,
            string mode,
            string expression,
            string result,
            string sourceValue = null,
            string sourceUnit = null,
            string targetUnit = null,
            string calculatorValue = null,
            string programmerRadix = null,
            string programmerBitLength = null,
            string scientificAngle = null,
            bool? scientificFToE = null)
        {
            if (string.IsNullOrWhiteSpace(sessionKey)
                || string.IsNullOrWhiteSpace(mode)
                || string.IsNullOrWhiteSpace(expression)
                || string.IsNullOrWhiteSpace(result))
            {
                return;
            }

            await _fileLock.WaitAsync();
            try
            {
                var entry = _entries.LastOrDefault(item => item.SessionKey == sessionKey);
                if (entry == null)
                {
                    entry = CreateEntry(
                        sessionKey,
                        mode,
                        expression,
                        result,
                        DateTimeOffset.UtcNow,
                        sourceValue,
                        sourceUnit,
                        targetUnit,
                        calculatorValue,
                        programmerRadix,
                        programmerBitLength,
                        scientificAngle,
                        scientificFToE);
                    _entries.Add(entry);
                    TrimEntries();
                }
                else
                {
                    entry.Mode = mode.Trim();
                    entry.Expression = expression.Trim();
                    entry.Result = result.Trim();
                    entry.SourceValue = NormalizeOptionalValue(sourceValue);
                    entry.SourceUnit = NormalizeOptionalValue(sourceUnit);
                    entry.TargetUnit = NormalizeOptionalValue(targetUnit);
                    entry.CalculatorValue = NormalizeOptionalValue(calculatorValue);
                    entry.ProgrammerRadix = NormalizeOptionalValue(programmerRadix);
                    entry.ProgrammerBitLength = NormalizeOptionalValue(programmerBitLength);
                    entry.ScientificAngle = NormalizeOptionalValue(scientificAngle);
                    entry.ScientificFToE = scientificFToE;
                    entry.TimestampUtc = DateTimeOffset.UtcNow;
                }

                await WriteDocumentLockedAsync();
            }
            catch (Exception ex)
            {
                LogSaveError(ex);
            }
            finally
            {
                _fileLock.Release();
            }
        }

        public async Task AppendAsync(string mode, string expression, string result)
        {
            if (string.IsNullOrWhiteSpace(mode)
                || string.IsNullOrWhiteSpace(expression)
                || string.IsNullOrWhiteSpace(result))
            {
                return;
            }

            await _fileLock.WaitAsync();
            try
            {
                _entries.Add(CreateEntry(
                    $"entry:{Guid.NewGuid():N}",
                    mode,
                    expression,
                    result,
                    DateTimeOffset.UtcNow));
                TrimEntries();
                await WriteDocumentLockedAsync();
            }
            catch (Exception ex)
            {
                LogSaveError(ex);
            }
            finally
            {
                _fileLock.Release();
            }
        }

        public async Task ClearAsync()
        {
            await _fileLock.WaitAsync();
            try
            {
                _entries.Clear();
                await WriteDocumentLockedAsync();
            }
            catch (Exception ex)
            {
                LogSaveError(ex);
            }
            finally
            {
                _fileLock.Release();
            }
        }

        private void ImportDocumentEntries(PersistentHistoryDocument document)
        {
            _entries.Clear();
            if (document.Entries == null)
            {
                return;
            }

            int index = 0;
            foreach (var source in document.Entries)
            {
                if (source == null
                    || string.IsNullOrWhiteSpace(source.Expression)
                    || string.IsNullOrWhiteSpace(source.Result))
                {
                    continue;
                }

                source.Id = string.IsNullOrWhiteSpace(source.Id) ? Guid.NewGuid().ToString("N") : source.Id;
                source.SessionKey = string.IsNullOrWhiteSpace(source.SessionKey)
                    ? $"import:{index++}:{source.Id}"
                    : source.SessionKey;
                source.Mode = string.IsNullOrWhiteSpace(source.Mode) ? "Standard" : source.Mode.Trim();
                source.Expression = source.Expression.Trim();
                source.Result = source.Result.Trim();
                source.SourceValue = NormalizeOptionalValue(source.SourceValue);
                source.SourceUnit = NormalizeOptionalValue(source.SourceUnit);
                source.TargetUnit = NormalizeOptionalValue(source.TargetUnit);
                source.CalculatorValue = NormalizeOptionalValue(source.CalculatorValue);
                source.ProgrammerRadix = NormalizeOptionalValue(source.ProgrammerRadix);
                source.ProgrammerBitLength = NormalizeOptionalValue(source.ProgrammerBitLength);
                source.ScientificAngle = NormalizeOptionalValue(source.ScientificAngle);
                source.TimestampUtc ??= document.SavedAtUtc == default
                    ? (DateTimeOffset?)null
                    : document.SavedAtUtc;
                _entries.Add(source);
            }
            TrimEntries();
        }

        private void MergeLegacyEntries(IEnumerable<PersistentHistoryEntry> imported)
        {
            foreach (var entry in imported.Reverse())
            {
                bool duplicate = _entries.Any(existing =>
                    string.Equals(existing.Mode, entry.Mode, StringComparison.OrdinalIgnoreCase)
                    && existing.Expression == entry.Expression
                    && existing.Result == entry.Result);
                if (!duplicate)
                {
                    _entries.Add(entry);
                }
            }
            TrimEntries();
        }

        private static IReadOnlyList<PersistentHistoryEntry> ParseLegacyHistory(string contents)
        {
            var result = new List<PersistentHistoryEntry>();
            if (string.IsNullOrWhiteSpace(contents))
            {
                return result;
            }

            var lines = contents
                .Split(new[] { "\r\n", "\n", "\r" }, StringSplitOptions.RemoveEmptyEntries)
                .Select(line => line.Trim())
                .Where(line => line.Length > 0 && !line.StartsWith("#") && !line.StartsWith(";"))
                .ToList();

            int index = 0;
            while (index < lines.Count)
            {
                string mode = "Standard";
                DateTimeOffset? timestamp = null;
                string line = lines[index++];

                if (TryParseLegacyMetadata(line, out var metadataMode, out var metadataTime))
                {
                    mode = metadataMode;
                    timestamp = metadataTime;
                    if (index >= lines.Count)
                    {
                        break;
                    }
                    line = lines[index++];
                }

                string expression;
                string value;
                int tab = line.IndexOf('\t');
                if (tab > 0)
                {
                    expression = line.Substring(0, tab).Trim();
                    value = line.Substring(tab + 1).Trim();
                }
                else if (line.EndsWith("=", StringComparison.Ordinal) && index < lines.Count)
                {
                    expression = line;
                    value = lines[index++];
                }
                else
                {
                    int equals = line.LastIndexOf('=');
                    if (equals <= 0 || equals == line.Length - 1)
                    {
                        continue;
                    }
                    expression = line.Substring(0, equals + 1).Trim();
                    value = line.Substring(equals + 1).Trim();
                }

                if (expression.Length > 0 && value.Length > 0)
                {
                    result.Add(CreateEntry(
                        $"legacy:{Guid.NewGuid():N}",
                        mode,
                        expression,
                        value,
                        timestamp));
                }
            }

            return result;
        }

        private static bool TryParseLegacyMetadata(
            string line,
            out string mode,
            out DateTimeOffset? timestamp)
        {
            mode = "Standard";
            timestamp = null;
            if (string.IsNullOrWhiteSpace(line) || line[0] != '[')
            {
                return false;
            }

            int close = line.IndexOf(']');
            if (close <= 1)
            {
                return false;
            }

            mode = line.Substring(1, close - 1).Trim();
            string dateText = line.Substring(close + 1).Trim();
            if (!dateText.Equals("Earlier", StringComparison.OrdinalIgnoreCase)
                && DateTime.TryParseExact(
                    dateText,
                    "yyyy-MM-dd HH:mm:ss",
                    CultureInfo.InvariantCulture,
                    DateTimeStyles.AssumeLocal,
                    out var localDate))
            {
                timestamp = new DateTimeOffset(localDate);
            }
            return true;
        }

        private static PersistentHistoryEntry CreateEntry(
            string sessionKey,
            string mode,
            string expression,
            string result,
            DateTimeOffset? timestamp,
            string sourceValue = null,
            string sourceUnit = null,
            string targetUnit = null,
            string calculatorValue = null,
            string programmerRadix = null,
            string programmerBitLength = null,
            string scientificAngle = null,
            bool? scientificFToE = null)
        {
            return new PersistentHistoryEntry
            {
                Id = Guid.NewGuid().ToString("N"),
                SessionKey = sessionKey,
                Mode = mode.Trim(),
                Expression = expression.Trim(),
                Result = result.Trim(),
                SourceValue = NormalizeOptionalValue(sourceValue),
                SourceUnit = NormalizeOptionalValue(sourceUnit),
                TargetUnit = NormalizeOptionalValue(targetUnit),
                CalculatorValue = NormalizeOptionalValue(calculatorValue),
                ProgrammerRadix = NormalizeOptionalValue(programmerRadix),
                ProgrammerBitLength = NormalizeOptionalValue(programmerBitLength),
                ScientificAngle = NormalizeOptionalValue(scientificAngle),
                ScientificFToE = scientificFToE,
                TimestampUtc = timestamp?.ToUniversalTime()
            };
        }

        private static string NormalizeOptionalValue(string value)
        {
            return string.IsNullOrWhiteSpace(value) ? null : value.Trim();
        }

        private void TrimEntries()
        {
            while (_entries.Count > MaximumEntries)
            {
                int oldestIndex = _entries
                    .Select((entry, index) => new { Entry = entry, Index = index })
                    .OrderBy(item => item.Entry.TimestampUtc ?? DateTimeOffset.MinValue)
                    .ThenBy(item => item.Index)
                    .Select(item => item.Index)
                    .First();
                _entries.RemoveAt(oldestIndex);
            }
        }

        private async Task WriteDocumentLockedAsync()
        {
            var document = new PersistentHistoryDocument
            {
                FormatVersion = CurrentFormatVersion,
                AppVersion = PersistentCalculatorVersion.Current,
                SavedAtUtc = DateTimeOffset.UtcNow,
                Entries = _entries.Select(entry => entry.Clone()).ToList(),
                HistoryState = _latestSnapshot == null ? null : new CalcManagerSnapshotAlias(_latestSnapshot)
            };

            var json = JsonSerializer.Serialize(document, _jsonOptions);
            var folder = await PersistentDataStore.GetFolderAsync();
            var pending = await folder.CreateFileAsync(
                "History.pending.txt",
                CreationCollisionOption.ReplaceExisting);
            await FileIO.WriteTextAsync(pending, json);

            var existing = await TryGetFileAsync(folder, PersistentDataStore.HistoryFileName);
            if (existing == null)
            {
                await pending.RenameAsync(
                    PersistentDataStore.HistoryFileName,
                    NameCollisionOption.ReplaceExisting);
            }
            else
            {
                await pending.MoveAndReplaceAsync(existing);
            }
        }

        private static async Task<StorageFile> TryGetFileAsync(StorageFolder folder, string name)
        {
            try
            {
                return await folder.GetFileAsync(name);
            }
            catch (FileNotFoundException)
            {
                return null;
            }
        }

        private static void LogSaveError(Exception ex)
        {
            ViewModel.Common.TraceLogger.GetInstance().LogRecallError(
                $"Persistent history could not be saved. {ex.GetType().Name}: {ex.Message}");
        }

        private sealed class PersistentHistoryDocument
        {
            [JsonPropertyName("formatVersion")]
            public int FormatVersion { get; set; }

            [JsonPropertyName("appVersion")]
            public string AppVersion { get; set; }

            [JsonPropertyName("savedAtUtc")]
            public DateTimeOffset SavedAtUtc { get; set; }

            [JsonPropertyName("entries")]
            public IReadOnlyList<PersistentHistoryEntry> Entries { get; set; }

            [JsonPropertyName("historyState")]
            public CalcManagerSnapshotAlias HistoryState { get; set; }
        }
    }

    internal static class PersistentCalculatorVersion
    {
        public const string Current = "1.0";
    }
}
