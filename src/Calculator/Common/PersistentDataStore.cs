// Licensed under the MIT License.

using System;
using System.IO;
using System.Threading.Tasks;

using Windows.Storage;

namespace CalculatorApp.Common
{
    internal static class PersistentDataStore
    {
        public const string FolderName = "Persistent Calculator";
        public const string HistoryFileName = "History.txt";
        public const string CurrencyMetadataFileName = "Currency Metadata.json";
        public const string CurrencyRatesFileName = "Currency Rates.json";
        public const string LegacyHistoryFileName = "Windows Calculator Saved History.txt";
        public const string LegacyCurrencyRatesFileName = "Windows Calculator Currency Rates.json";

        public static async Task<StorageFolder> GetFolderAsync()
        {
            // KnownFolders.DocumentsLibrary respects the user's configured Documents
            // location, including OneDrive redirection. Package.appxmanifest limits this
            // app to the .txt and .json file types used by Persistent Calculator.
            return await KnownFolders.DocumentsLibrary.CreateFolderAsync(
                FolderName,
                CreationCollisionOption.OpenIfExists);
        }

        public static async Task<StorageFile> TryGetLegacyHistoryFileAsync()
        {
            return await TryGetFileAsync(KnownFolders.DocumentsLibrary, LegacyHistoryFileName);
        }

        public static async Task DeleteLegacyHistoryFileAsync()
        {
            await DeleteIfPresentAsync(KnownFolders.DocumentsLibrary, LegacyHistoryFileName);
        }

        public static async Task DeleteLegacyCurrencyRatesFileAsync()
        {
            await DeleteIfPresentAsync(KnownFolders.DocumentsLibrary, LegacyCurrencyRatesFileName);
        }

        private static async Task DeleteIfPresentAsync(StorageFolder folder, string name)
        {
            var file = await TryGetFileAsync(folder, name);
            if (file != null)
            {
                await file.DeleteAsync(StorageDeleteOption.PermanentDelete);
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
    }
}
