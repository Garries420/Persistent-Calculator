#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/update_parser.h"

int main(void) {
    static const char *valid_json =
        "{\"tag_name\": \"v1.2.3\", \"assets\": ["
        "{\"name\": \"PersistentCalculator.exe\","
        "\"digest\": \"sha256:0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF\","
        "\"browser_download_url\": "
        "\"https://github.com/Garries420/Persistent-Calculator/releases/download/v1.2.3/PersistentCalculator.exe\"}]}";
    UpdateReleaseInfo release;
    unsigned version[3];
    char changed[4096];

    assert(update_parse_version("1.0.0", version));
    assert(version[0] == 1 && version[1] == 0 && version[2] == 0);
    assert(update_parse_version("v12.34.56", version));
    assert(!update_parse_version("1.0", version));
    assert(!update_parse_version("1.0.0-beta", version));
    assert(update_compare_versions("1.0.1", "1.0.0") > 0);
    assert(update_compare_versions("2.0.0", "1.99.99") > 0);
    assert(update_compare_versions("1.0.0", "1.0.0") == 0);
    assert(update_compare_versions("0.9.9", "1.0.0") < 0);

    assert(update_parse_release_json(valid_json, &release));
    assert(strcmp(release.version, "1.2.3") == 0);
    assert(strcmp(release.sha256,
                  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") == 0);

    snprintf(changed, sizeof(changed), "%s", valid_json);
    {
        char *domain = strstr(changed, "github.com");
        assert(domain);
        memcpy(domain, "evilhub.io", 10);
    }
    assert(!update_parse_release_json(changed, &release));

    snprintf(changed, sizeof(changed), "%s", valid_json);
    {
        char *digest = strstr(changed, "0123456789ABCDEF");
        assert(digest);
        digest[0] = 'Z';
    }
    assert(!update_parse_release_json(changed, &release));

    snprintf(changed, sizeof(changed), "%s", valid_json);
    {
        char *asset = strstr(changed, "PersistentCalculator.exe");
        assert(asset);
        asset[0] = 'X';
    }
    assert(!update_parse_release_json(changed, &release));

    puts("All updater parser tests passed.");
    return 0;
}
