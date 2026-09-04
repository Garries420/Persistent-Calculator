// Licensed under the MIT License.

using System;
using System.Threading.Tasks;

using Windows.Web.Http;

namespace CalculatorApp.Common
{
    internal static class UpdateService
    {
        private const string LatestReleaseApi =
            "https://api.github.com/repos/Garries420/Persistent-Calculator/releases/latest";
        private const int MaximumReleaseResponseCharacters = 256 * 1024;
        internal static readonly Uri LatestReleasePage =
            new Uri("https://github.com/Garries420/Persistent-Calculator/releases/latest");

        public static async Task<UpdateCheckResult> CheckAsync()
        {
            try
            {
                using (var client = new HttpClient())
                {
                    client.DefaultRequestHeaders.UserAgent.TryParseAdd(
                        $"PersistentCalculator/{PersistentCalculatorVersion.Current}");
                    client.DefaultRequestHeaders.Accept.TryParseAdd("application/vnd.github+json");

                    var response = await client.GetAsync(new Uri(LatestReleaseApi));
                    // GitHub returns 404 when a repository has no published
                    // releases yet. That means there is no newer release, not
                    // that the user's current installation is broken.
                    if (response.StatusCode == HttpStatusCode.NotFound)
                    {
                        return new UpdateCheckResult
                        {
                            Status = UpdateCheckStatus.UpToDate,
                            Version = PersistentCalculatorVersion.Current
                        };
                    }

                    if (!response.IsSuccessStatusCode || response.Content == null)
                    {
                        return Failed();
                    }

                    var json = await response.Content.ReadAsStringAsync();
                    if (string.IsNullOrWhiteSpace(json) || json.Length > MaximumReleaseResponseCharacters)
                    {
                        return Failed();
                    }

                    return UpdateReleaseParser.Parse(json, PersistentCalculatorVersion.Current);
                }
            }
            catch
            {
                return Failed();
            }
        }

        private static UpdateCheckResult Failed()
        {
            return new UpdateCheckResult { Status = UpdateCheckStatus.Failed };
        }
    }
}
