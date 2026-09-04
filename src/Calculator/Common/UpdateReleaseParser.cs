// Licensed under the MIT License.

using System;
using System.Globalization;
using System.Text.Json;

namespace CalculatorApp.Common
{
    internal enum UpdateCheckStatus
    {
        UpToDate,
        UpdateAvailable,
        Failed
    }

    internal sealed class UpdateCheckResult
    {
        public UpdateCheckStatus Status { get; set; }
        public string Version { get; set; }
        public string ReleaseNotes { get; set; }
        public Uri ReleasePage { get; set; }
    }

    internal static class UpdateReleaseParser
    {
        private const int MaximumReleaseNotesCharacters = 64 * 1024;
        private static readonly DateTimeOffset ReleaseLineStartedAtUtc =
            new DateTimeOffset(2026, 9, 1, 22, 0, 0, TimeSpan.Zero);

        internal static UpdateCheckResult Parse(string json, string currentVersionText)
        {
            if (string.IsNullOrWhiteSpace(json)
                || !Version.TryParse(currentVersionText, out var currentVersion))
            {
                return Failed();
            }

            try
            {
                using (var document = JsonDocument.Parse(json))
                {
                    var root = document.RootElement;
                    if (GetBoolean(root, "draft") || GetBoolean(root, "prerelease"))
                    {
                        return Failed();
                    }

                    // Version 1.0 starts a new release line. Ignore the higher
                    // numbered legacy releases that predate this reset so the new
                    // app never advertises an older calculator as an update.
                    if (!DateTimeOffset.TryParse(
                            GetString(root, "published_at"),
                            CultureInfo.InvariantCulture,
                            DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal,
                            out DateTimeOffset publishedAt)
                        || publishedAt < ReleaseLineStartedAtUtc)
                    {
                        return UpToDate(currentVersionText);
                    }

                    var tag = GetString(root, "tag_name")?.Trim();
                    var versionText = tag?.TrimStart('v', 'V');
                    if (!Version.TryParse(versionText, out var latestVersion))
                    {
                        return Failed();
                    }

                    // A GitHub product tag may use 1.1 while an MSIX package uses
                    // 1.1.0.0. Missing components represent zero, not a newer build.
                    var latest = NormalizeVersion(latestVersion);
                    var current = NormalizeVersion(currentVersion);
                    if (latest <= current)
                    {
                        return UpToDate(versionText);
                    }

                    string releaseNotes = GetString(root, "body")?.Trim() ?? string.Empty;
                    if (releaseNotes.Length > MaximumReleaseNotesCharacters)
                    {
                        releaseNotes = releaseNotes.Substring(0, MaximumReleaseNotesCharacters);
                    }

                    return new UpdateCheckResult
                    {
                        Status = UpdateCheckStatus.UpdateAvailable,
                        Version = versionText,
                        ReleaseNotes = releaseNotes,
                        ReleasePage = CreateAllowedUri(GetString(root, "html_url"))
                    };
                }
            }
            catch
            {
                return Failed();
            }
        }

        private static Version NormalizeVersion(Version version)
        {
            return new Version(
                version.Major,
                version.Minor,
                Math.Max(0, version.Build),
                Math.Max(0, version.Revision));
        }

        private static Uri CreateAllowedUri(string value)
        {
            if (!Uri.TryCreate(value, UriKind.Absolute, out var uri)
                || !uri.Scheme.Equals(Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase)
                || !uri.Host.Equals("github.com", StringComparison.OrdinalIgnoreCase))
            {
                return null;
            }

            const string expectedPath = "/Garries420/Persistent-Calculator/";
            return uri.AbsolutePath.StartsWith(expectedPath, StringComparison.OrdinalIgnoreCase) ? uri : null;
        }

        private static string GetString(JsonElement value, string propertyName)
        {
            return value.TryGetProperty(propertyName, out var property) && property.ValueKind == JsonValueKind.String
                ? property.GetString()
                : null;
        }

        private static bool GetBoolean(JsonElement value, string propertyName)
        {
            return value.TryGetProperty(propertyName, out var property)
                && property.ValueKind == JsonValueKind.True;
        }

        private static UpdateCheckResult Failed()
        {
            return new UpdateCheckResult { Status = UpdateCheckStatus.Failed };
        }

        private static UpdateCheckResult UpToDate(string version)
        {
            return new UpdateCheckResult
            {
                Status = UpdateCheckStatus.UpToDate,
                Version = version
            };
        }
    }
}
