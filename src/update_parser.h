#ifndef UPDATE_PARSER_H
#define UPDATE_PARSER_H

#include <stddef.h>

typedef struct UpdateReleaseInfo {
    char version[32];
    char download_url[2048];
    char sha256[65];
} UpdateReleaseInfo;

int update_parse_version(const char *text, unsigned version[3]);
int update_compare_versions(const char *candidate, const char *current);
int update_parse_release_json(const char *json, UpdateReleaseInfo *release);

#endif
