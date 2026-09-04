// Licensed under the MIT License.

using System.Text.Json;

using CalculatorApp.Common;

using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UpdateReleaseParserTests
{
    [TestClass]
    public class ReleaseParserTests
    {
        [TestMethod]
        public void SameVersionWithFourPartsIsUpToDate()
        {
            var result = Parse("v1.0.0.0");

            Assert.AreEqual(UpdateCheckStatus.UpToDate, result.Status);
        }

        [TestMethod]
        public void NewStableVersionIncludesReleaseNotesAndPage()
        {
            var result = Parse("v1.1", body: "Added a new history view.");

            Assert.AreEqual(UpdateCheckStatus.UpdateAvailable, result.Status);
            Assert.AreEqual("1.1", result.Version);
            Assert.AreEqual("Added a new history view.", result.ReleaseNotes);
            Assert.AreEqual(
                "https://github.com/Garries420/Persistent-Calculator/releases/tag/v1.1",
                result.ReleasePage.AbsoluteUri.TrimEnd('/'));
        }

        [TestMethod]
        public void ReleasePageUsesOnlyTheOfficialRepository()
        {
            var result = Parse("v1.1");

            Assert.AreEqual(UpdateCheckStatus.UpdateAvailable, result.Status);
            Assert.AreEqual(
                "https://github.com/Garries420/Persistent-Calculator/releases/tag/v1.1",
                result.ReleasePage.AbsoluteUri.TrimEnd('/'));
        }

        [TestMethod]
        public void DraftAndPrereleaseAreRejected()
        {
            Assert.AreEqual(UpdateCheckStatus.Failed, Parse("v1.1", draft: true).Status);
            Assert.AreEqual(UpdateCheckStatus.Failed, Parse("v1.1", prerelease: true).Status);
        }

        [TestMethod]
        public void OldLegacyReleaseIsIgnored()
        {
            var result = Parse("v99.0", publishedAt: "2026-08-01T00:00:00Z");

            Assert.AreEqual(UpdateCheckStatus.UpToDate, result.Status);
        }

        [TestMethod]
        public void UntrustedDestinationsAreRejected()
        {
            string json = JsonSerializer.Serialize(new
            {
                tag_name = "v1.1",
                draft = false,
                prerelease = false,
                published_at = "2026-09-04T08:00:00Z",
                html_url = "https://example.com/releases/tag/v1.1",
                body = "Notes",
                assets = new[]
                {
                    new
                    {
                        name = "PersistentCalculator.msixbundle",
                        browser_download_url = "https://example.com/PersistentCalculator.msixbundle"
                    }
                }
            });

            var result = UpdateReleaseParser.Parse(json, "1.0");

            Assert.AreEqual(UpdateCheckStatus.UpdateAvailable, result.Status);
            Assert.IsNull(result.ReleasePage);
        }

        private static UpdateCheckResult Parse(
            string tag,
            bool draft = false,
            bool prerelease = false,
            string publishedAt = "2026-09-04T08:00:00Z",
            string body = "Release notes",
            object[] assets = null)
        {
            string json = JsonSerializer.Serialize(new
            {
                tag_name = tag,
                draft,
                prerelease,
                published_at = publishedAt,
                html_url = $"https://github.com/Garries420/Persistent-Calculator/releases/tag/{tag}",
                body,
                assets = assets ?? new object[0]
            });

            return UpdateReleaseParser.Parse(json, "1.0");
        }
    }
}
