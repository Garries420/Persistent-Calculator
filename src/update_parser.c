#include "update_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define RELEASE_ASSET_NAME "PersistentCalculator.exe"
#define RELEASE_URL_PREFIX \
    "https://github.com/Garries420/Persistent-Calculator/releases/download/"

static const char *skip_space(const char *cursor) {
    while (*cursor && isspace((unsigned char)*cursor)) ++cursor;
    return cursor;
}

static int json_string_for_key(const char *start, const char *key,
                               char *output, size_t capacity,
                               const char **value_end) {
    char pattern[96];
    const char *cursor;
    size_t used = 0;
    if (!start || !key || !output || capacity == 0) return 0;
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) < 0) return 0;
    cursor = strstr(start, pattern);
    if (!cursor) return 0;
    cursor += strlen(pattern);
    cursor = skip_space(cursor);
    if (*cursor++ != ':') return 0;
    cursor = skip_space(cursor);
    if (*cursor++ != '"') return 0;
    while (*cursor && *cursor != '"') {
        unsigned char ch = (unsigned char)*cursor++;
        if (ch == '\\') {
            ch = (unsigned char)*cursor++;
            switch (ch) {
                case '"': case '\\': case '/': break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                default: return 0;
            }
        }
        if (ch < 0x20 || used + 1 >= capacity) return 0;
        output[used++] = (char)ch;
    }
    if (*cursor != '"') return 0;
    output[used] = '\0';
    if (value_end) *value_end = cursor + 1;
    return 1;
}

static int valid_sha256(const char *value) {
    size_t index;
    if (!value || strlen(value) != 64) return 0;
    for (index = 0; index < 64; ++index)
        if (!isxdigit((unsigned char)value[index])) return 0;
    return 1;
}

int update_parse_version(const char *text, unsigned version[3]) {
    const char *cursor = text;
    char tail;
    if (!cursor || !version) return 0;
    if (*cursor == 'v' || *cursor == 'V') ++cursor;
    if (sscanf(cursor, "%u.%u.%u%c", &version[0], &version[1], &version[2], &tail) != 3)
        return 0;
    return version[0] <= 9999 && version[1] <= 9999 && version[2] <= 9999;
}

int update_compare_versions(const char *candidate, const char *current) {
    unsigned left[3], right[3];
    int index;
    if (!update_parse_version(candidate, left) || !update_parse_version(current, right))
        return 0;
    for (index = 0; index < 3; ++index) {
        if (left[index] > right[index]) return 1;
        if (left[index] < right[index]) return -1;
    }
    return 0;
}

int update_parse_release_json(const char *json, UpdateReleaseInfo *release) {
    const char *cursor;
    const char *next_name;
    const char *url_key;
    const char *digest_key;
    char tag[32];
    char name[128];
    char digest[96];
    unsigned parsed[3];
    if (!json || !release) return 0;
    memset(release, 0, sizeof(*release));
    if (!json_string_for_key(json, "tag_name", tag, sizeof(tag), NULL) ||
        !update_parse_version(tag, parsed)) return 0;
    snprintf(release->version, sizeof(release->version), "%u.%u.%u",
             parsed[0], parsed[1], parsed[2]);

    cursor = json;
    while ((cursor = strstr(cursor, "\"name\"")) != NULL) {
        const char *after_name = NULL;
        if (!json_string_for_key(cursor, "name", name, sizeof(name), &after_name)) return 0;
        if (strcmp(name, RELEASE_ASSET_NAME) == 0) {
            next_name = strstr(after_name, "\"name\"");
            url_key = strstr(after_name, "\"browser_download_url\"");
            digest_key = strstr(after_name, "\"digest\"");
            if (!url_key || !digest_key ||
                (next_name && (url_key > next_name || digest_key > next_name))) return 0;
            if (!json_string_for_key(url_key, "browser_download_url",
                                     release->download_url,
                                     sizeof(release->download_url), NULL) ||
                strncmp(release->download_url, RELEASE_URL_PREFIX,
                        strlen(RELEASE_URL_PREFIX)) != 0) return 0;
            {
                size_t url_length = strlen(release->download_url);
                static const char suffix[] = "/" RELEASE_ASSET_NAME;
                if (url_length < sizeof(suffix) - 1 ||
                    strcmp(release->download_url + url_length - (sizeof(suffix) - 1),
                           suffix) != 0) return 0;
            }
            if (!json_string_for_key(digest_key, "digest", digest, sizeof(digest), NULL) ||
                strncmp(digest, "sha256:", 7) != 0 || !valid_sha256(digest + 7)) return 0;
            for (size_t index = 0; index < 64; ++index)
                release->sha256[index] = (char)tolower((unsigned char)digest[index + 7]);
            release->sha256[64] = '\0';
            return 1;
        }
        cursor = after_name;
    }
    return 0;
}
