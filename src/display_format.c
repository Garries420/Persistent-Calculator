#include "display_format.h"

#include <string.h>

static void append_wide(wchar_t *destination, size_t capacity,
                        size_t *used, wchar_t character) {
    if (*used + 1 < capacity) destination[(*used)++] = character;
}

void display_format_ascii_number(const char *source, wchar_t *destination,
                                 size_t capacity) {
    const char *integer_start;
    const char *integer_end;
    const char *cursor;
    size_t integer_length;
    size_t used = 0;
    size_t index;
    if (!destination || capacity == 0) return;
    destination[0] = L'\0';
    if (!source) return;

    cursor = source;
    if (*cursor == '+' || *cursor == '-') {
        append_wide(destination, capacity, &used,
                    *cursor == '-' ? L'−' : L'+');
        ++cursor;
    }
    integer_start = cursor;
    while (*cursor >= '0' && *cursor <= '9') ++cursor;
    integer_end = cursor;
    integer_length = (size_t)(integer_end - integer_start);
    for (index = 0; index < integer_length; ++index) {
        if (index > 0 && (integer_length - index) % 3 == 0)
            append_wide(destination, capacity, &used, L' ');
        append_wide(destination, capacity, &used,
                    (wchar_t)integer_start[index]);
    }

    while (*cursor) {
        unsigned char character = (unsigned char)*cursor++;
        if (character == '.') append_wide(destination, capacity, &used, L',');
        else if (character == '*') append_wide(destination, capacity, &used, L'×');
        else if (character == '/') append_wide(destination, capacity, &used, L'÷');
        else if (character == '-') append_wide(destination, capacity, &used, L'−');
        else append_wide(destination, capacity, &used, (wchar_t)character);
    }
    destination[used] = L'\0';
}
