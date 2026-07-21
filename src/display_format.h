#ifndef DISPLAY_FORMAT_H
#define DISPLAY_FORMAT_H

#include <stddef.h>
#include <wchar.h>

void display_format_ascii_number(const char *source, wchar_t *destination,
                                 size_t capacity);

#endif
