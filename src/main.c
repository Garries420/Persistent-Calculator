#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "calc_engine.h"
#include "display_format.h"
#include "updater.h"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#define APP_CLASS L"PersistentCalculatorMainWindow"
#define APP_TITLE L"Calculator"
#define HISTORY_CAPACITY 500
#define PATH_CAPACITY 32768

#define ID_GRID_BASE 100
#define ID_MEMORY_BASE 200
#define ID_HISTORY 300
#define ID_MENU 301
#define ID_CLEAR_HISTORY 401
#define ID_HISTORY_ITEM_BASE 500
#define ID_NAV_STANDARD 600
#define ID_CHECK_UPDATES 601
#define ID_NAV_CHANGELOG 602
#define ID_MEMORY_VALUE 700
#define ID_ACTIVE_SCROLLBAR 800
#define ID_HISTORY_SCROLLBAR_BASE 900
#define ID_COPY_RESULT 1000
#define ID_CHANGELOG_TAB_BASE 1100
#define MAX_VISIBLE_HISTORY 32
#define CHANGELOG_MAX_RELEASES 5

typedef struct HistoryEntry {
    wchar_t expression[2048];
    wchar_t result[160];
    int scroll_x;
} HistoryEntry;

typedef struct GridButton {
    const wchar_t *label;
    int kind;
} GridButton;

typedef struct SavedWindowPlacement {
    DWORD version;
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
    DWORD maximized;
} SavedWindowPlacement;

typedef struct ChangelogRelease {
    const wchar_t *version;
    const wchar_t *date;
    const wchar_t *const *items;
    int item_count;
} ChangelogRelease;

static const GridButton g_buttons[24] = {
    {L"%", 0}, {L"CE", 0}, {L"C", 0}, {L"⌫", 0},
    {L"1/x", 0}, {L"x²", 0}, {L"²√x", 0}, {L"÷", 0},
    {L"7", 1}, {L"8", 1}, {L"9", 1}, {L"×", 0},
    {L"4", 1}, {L"5", 1}, {L"6", 1}, {L"−", 0},
    {L"1", 1}, {L"2", 1}, {L"3", 1}, {L"+", 0},
    {L"+/−", 1}, {L"0", 1}, {L",", 1}, {L"=", 2}
};

static const wchar_t *g_memory_labels[6] = {L"MC", L"MR", L"M+", L"M−", L"MS", L"M⌄"};

static const wchar_t *const g_changelog_110[] = {
    L"Large totals now use readable three-digit spacing, such as 5 000 and 5 000 000.",
    L"The changelog keeps up to the five latest releases, with version tabs and vertical scrolling.",
    L"Routine update-status notices now disappear after two seconds.",
    L"Available updates ask for permission before downloading, so the calculator can be used immediately.",
    L"Accepted updates now show private-safe download, verification, and installation progress."
};

static const wchar_t *const g_changelog_101[] = {
    L"Update notices stay attached to the calculator window when it moves.",
    L"Update-status messages wrap cleanly so their complete text remains readable.",
    L"Added an in-app changelog screen to the hamburger navigation menu."
};

static const wchar_t *const g_changelog_100[] = {
    L"Initial public release with permanent calculation history.",
    L"Preserved expression chains can be recalled and continued.",
    L"Selectable results, remembered window placement, percentage calculations, and verified automatic updates."
};

/* Keep this list newest-first and retain at most the latest five releases. */
static const ChangelogRelease g_changelog_releases[] = {
    {L"1.1.0", L"21 July 2026", g_changelog_110, (int)_countof(g_changelog_110)},
    {L"1.0.1", L"20 July 2026", g_changelog_101, (int)_countof(g_changelog_101)},
    {L"1.0.0", L"20 July 2026", g_changelog_100, (int)_countof(g_changelog_100)}
};

static CalcState g_calc;
static HistoryEntry g_history[HISTORY_CAPACITY];
static int g_history_count;
static int g_history_scroll;
static int g_history_open;
static int g_calculation_history_index = -1;
static int g_nav_open;
static int g_changelog_open;
static int g_changelog_selected;
static int g_changelog_scroll;
static int g_changelog_scroll_max;
static int g_memory_popup;
static int g_hot_id = -1;
static int g_pressed_id = -1;
static UINT g_dpi = 96;
static wchar_t g_history_path[PATH_CAPACITY];
static int g_active_scroll_x;
static int g_active_scroll_max;
static RECT g_active_scroll_track;
static RECT g_active_scroll_thumb;
static wchar_t g_last_active_expression[2048];
static int g_history_scroll_maximum[MAX_VISIBLE_HISTORY];
static int g_history_scroll_entry[MAX_VISIBLE_HISTORY];
static RECT g_history_scroll_track[MAX_VISIBLE_HISTORY];
static RECT g_history_scroll_thumb[MAX_VISIBLE_HISTORY];
static int g_scroll_drag_id = -1;
static int g_scroll_drag_start_x;
static int g_scroll_drag_start_thumb_left;
static int g_result_selecting;
static int g_result_selection_anchor = -1;
static int g_result_selection_end = -1;
static wchar_t g_result_selection_text[160];

static HFONT g_font_normal;
static HFONT g_font_small;
static HFONT g_font_expression;
static HFONT g_font_title;
static HFONT g_font_display;
static HFONT g_font_display_small;
static HFONT g_font_history_result;

static COLORREF CLR_BACKGROUND = RGB(32, 32, 32);
static COLORREF COLOR_PANEL = RGB(37, 37, 37);
static COLORREF COLOR_BUTTON = RGB(50, 50, 50);
static COLORREF COLOR_NUMBER = RGB(59, 59, 59);
static COLORREF COLOR_HOVER = RGB(69, 69, 69);
static COLORREF COLOR_PRESSED = RGB(78, 78, 78);
static COLORREF COLOR_EQUAL = RGB(156, 198, 217);
static COLORREF COLOR_EQUAL_HOVER = RGB(176, 213, 230);
static COLORREF COLOR_TEXT = RGB(246, 246, 246);
static COLORREF COLOR_MUTED = RGB(170, 175, 176);
static COLORREF COLOR_DISABLED = RGB(103, 108, 109);
static COLORREF COLOR_DANGER = RGB(238, 92, 92);
static COLORREF COLOR_RESULT_SELECTION = RGB(82, 82, 82);

static int scale_px(int value) {
    return MulDiv(value, (int)g_dpi, 96);
}

static int load_window_placement(RECT *rect, int *maximized) {
    HKEY key;
    SavedWindowPlacement saved;
    DWORD type = 0;
    DWORD size = sizeof(saved);
    RECT candidate;
    HMONITOR monitor;
    MONITORINFO info;
    int width, height;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\PersistentCalculator", 0,
                      KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return 0;
    if (RegQueryValueExW(key, L"WindowPlacement", NULL, &type, (BYTE *)&saved, &size) !=
            ERROR_SUCCESS ||
        type != REG_BINARY || size != sizeof(saved) || saved.version != 1) {
        RegCloseKey(key);
        return 0;
    }
    RegCloseKey(key);
    candidate.left = saved.left;
    candidate.top = saved.top;
    candidate.right = saved.right;
    candidate.bottom = saved.bottom;
    width = candidate.right - candidate.left;
    height = candidate.bottom - candidate.top;
    if (width < 250 || height < 400 || width > 10000 || height > 10000) return 0;
    monitor = MonitorFromRect(&candidate, MONITOR_DEFAULTTONULL);
    if (!monitor) return 0;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return 0;
    /* Keep the remembered location, but guarantee that the title bar remains
       reachable after a monitor or resolution change. */
    if (candidate.left > info.rcWork.right - 80)
        OffsetRect(&candidate, info.rcWork.right - 80 - candidate.left, 0);
    if (candidate.right < info.rcWork.left + 80)
        OffsetRect(&candidate, info.rcWork.left + 80 - candidate.right, 0);
    if (candidate.top < info.rcWork.top)
        OffsetRect(&candidate, 0, info.rcWork.top - candidate.top);
    if (candidate.top > info.rcWork.bottom - 32)
        OffsetRect(&candidate, 0, info.rcWork.bottom - 32 - candidate.top);
    *rect = candidate;
    *maximized = saved.maximized != 0;
    return 1;
}

static void save_window_placement(HWND window) {
    WINDOWPLACEMENT placement;
    SavedWindowPlacement saved;
    HKEY key;
    DWORD disposition;
    memset(&placement, 0, sizeof(placement));
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(window, &placement)) return;
    saved.version = 1;
    saved.left = placement.rcNormalPosition.left;
    saved.top = placement.rcNormalPosition.top;
    saved.right = placement.rcNormalPosition.right;
    saved.bottom = placement.rcNormalPosition.bottom;
    saved.maximized = placement.showCmd == SW_SHOWMAXIMIZED ||
                      (placement.showCmd == SW_SHOWMINIMIZED &&
                       (placement.flags & WPF_RESTORETOMAXIMIZED));
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\PersistentCalculator", 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &key, &disposition) == ERROR_SUCCESS) {
        RegSetValueExW(key, L"WindowPlacement", 0, REG_BINARY, (const BYTE *)&saved,
                       sizeof(saved));
        RegCloseKey(key);
    }
}

static HFONT create_font(int points, int weight) {
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -MulDiv(points, (int)g_dpi, 72);
    lf.lfWeight = weight;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(lf.lfFaceName, L"Segoe UI");
    return CreateFontIndirectW(&lf);
}

static void destroy_fonts(void) {
    if (g_font_normal) DeleteObject(g_font_normal);
    if (g_font_small) DeleteObject(g_font_small);
    if (g_font_expression) DeleteObject(g_font_expression);
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_display) DeleteObject(g_font_display);
    if (g_font_display_small) DeleteObject(g_font_display_small);
    if (g_font_history_result) DeleteObject(g_font_history_result);
    g_font_normal = g_font_small = g_font_expression = g_font_title = NULL;
    g_font_display = g_font_display_small = g_font_history_result = NULL;
}

static void create_fonts(void) {
    destroy_fonts();
    g_font_normal = create_font(12, FW_NORMAL);
    g_font_small = create_font(9, FW_NORMAL);
    g_font_expression = create_font(11, FW_NORMAL);
    g_font_title = create_font(14, FW_SEMIBOLD);
    g_font_display = create_font(35, FW_SEMIBOLD);
    g_font_display_small = create_font(21, FW_SEMIBOLD);
    g_font_history_result = create_font(18, FW_SEMIBOLD);
}

static void fill_rect_color(HDC dc, const RECT *rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void rounded_rect(HDC dc, const RECT *rect, COLORREF color, int radius) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect->left, rect->top, rect->right, rect->bottom, radius, radius);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void draw_text_color(HDC dc, const wchar_t *text_value, RECT rect, HFONT font,
                            COLORREF color, UINT format) {
    HGDIOBJ old_font = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text_value, -1, &rect, format | DT_NOPREFIX);
    SelectObject(dc, old_font);
}

static void clear_result_selection(void) {
    g_result_selecting = 0;
    g_result_selection_anchor = -1;
    g_result_selection_end = -1;
    g_result_selection_text[0] = L'\0';
}

static void sync_result_selection(const wchar_t *text_value) {
    int length;
    if (g_result_selection_anchor < 0) return;
    if (wcscmp(g_result_selection_text, text_value) != 0) {
        clear_result_selection();
        return;
    }
    length = (int)wcslen(text_value);
    if (g_result_selection_anchor > length) g_result_selection_anchor = length;
    if (g_result_selection_end > length) g_result_selection_end = length;
}

static HFONT get_auto_fit_font(HDC dc, const wchar_t *text_value, RECT rect, HFONT base_font,
                               int base_points, int minimum_points, int weight,
                               HFONT *owned_font) {
    HFONT fitted_font = NULL;
    HGDIOBJ old_font;
    SIZE size = {0, 0};
    int available = rect.right - rect.left;
    int points = base_points;
    *owned_font = NULL;
    old_font = SelectObject(dc, base_font);
    GetTextExtentPoint32W(dc, text_value, (int)wcslen(text_value), &size);
    SelectObject(dc, old_font);
    if (size.cx > available && size.cx > 0) {
        points = (base_points * available) / size.cx;
        if (points >= base_points) points = base_points - 1;
        if (points < minimum_points) points = minimum_points;
        do {
            if (fitted_font) DeleteObject(fitted_font);
            fitted_font = create_font(points, weight);
            if (!fitted_font) break;
            old_font = SelectObject(dc, fitted_font);
            GetTextExtentPoint32W(dc, text_value, (int)wcslen(text_value), &size);
            SelectObject(dc, old_font);
            if (size.cx <= available || points <= minimum_points) break;
            --points;
        } while (1);
    }
    *owned_font = fitted_font;
    return fitted_font ? fitted_font : base_font;
}

static void draw_auto_fit_text(HDC dc, const wchar_t *text_value, RECT rect, HFONT base_font,
                               int base_points, int minimum_points, int weight,
                               COLORREF color) {
    HFONT fitted_font = NULL;
    HFONT font = get_auto_fit_font(dc, text_value, rect, base_font, base_points,
                                   minimum_points, weight, &fitted_font);
    draw_text_color(dc, text_value, rect, font, color,
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    if (fitted_font) DeleteObject(fitted_font);
}

static void draw_selectable_display(HDC dc, const wchar_t *text_value, RECT rect) {
    HFONT fitted_font = NULL;
    HFONT font;
    HGDIOBJ old_font;
    SIZE full_size = {0, 0};
    SIZE start_size = {0, 0};
    SIZE end_size = {0, 0};
    TEXTMETRICW metrics;
    int start, end, text_x;
    sync_result_selection(text_value);
    font = get_auto_fit_font(dc, text_value, rect, g_font_display, 35, 8,
                             FW_SEMIBOLD, &fitted_font);
    old_font = SelectObject(dc, font);
    GetTextExtentPoint32W(dc, text_value, (int)wcslen(text_value), &full_size);
    text_x = rect.right - full_size.cx;
    start = g_result_selection_anchor;
    end = g_result_selection_end;
    if (start >= 0 && end >= 0 && start != end) {
        RECT selection;
        if (start > end) {
            int swap = start;
            start = end;
            end = swap;
        }
        if (start > 0) GetTextExtentPoint32W(dc, text_value, start, &start_size);
        if (end > 0) GetTextExtentPoint32W(dc, text_value, end, &end_size);
        GetTextMetricsW(dc, &metrics);
        selection.left = text_x + start_size.cx;
        selection.right = text_x + end_size.cx;
        selection.top = (rect.top + rect.bottom - metrics.tmHeight) / 2;
        selection.bottom = selection.top + metrics.tmHeight;
        if (selection.left < rect.left) selection.left = rect.left;
        if (selection.right > rect.right) selection.right = rect.right;
        if (selection.right > selection.left)
            fill_rect_color(dc, &selection, COLOR_RESULT_SELECTION);
    }
    SetTextColor(dc, COLOR_TEXT);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text_value, -1, &rect,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old_font);
    if (fitted_font) DeleteObject(fitted_font);
}

static RECT grid_rect(int index, int width, int height) {
    int padding = scale_px(4);
    int gap = scale_px(2);
    int grid_height = scale_px(350);
    int top = height - grid_height - padding;
    int usable_width = width - padding * 2 - gap * 3;
    int usable_height = grid_height - gap * 5;
    int column = index % 4;
    int row = index / 4;
    int left = padding + (usable_width * column) / 4 + gap * column;
    int right = padding + (usable_width * (column + 1)) / 4 + gap * column;
    int y1 = top + (usable_height * row) / 6 + gap * row;
    int y2 = top + (usable_height * (row + 1)) / 6 + gap * row;
    RECT rect = {left, y1, right, y2};
    return rect;
}

static int grid_top(int height) {
    return height - scale_px(350) - scale_px(4);
}

static RECT result_display_rect(int width, int height) {
    RECT rect = {scale_px(10), scale_px(82), width - scale_px(13),
                 grid_top(height) - scale_px(40)};
    return rect;
}

static RECT memory_rect(int index, int width, int height) {
    int top = grid_top(height) - scale_px(40);
    RECT rect = {(width * index) / 6, top, (width * (index + 1)) / 6, grid_top(height)};
    return rect;
}

static RECT header_history_rect(int width) {
    RECT rect = {width - scale_px(52), scale_px(6), width - scale_px(4), scale_px(48)};
    return rect;
}

static RECT header_menu_rect(void) {
    RECT rect = {scale_px(3), scale_px(6), scale_px(43), scale_px(48)};
    return rect;
}

static void ascii_to_pretty(const char *source, wchar_t *destination, size_t capacity) {
    size_t out = 0;
    if (!destination || capacity == 0) return;
    while (*source && out + 1 < capacity) {
        unsigned char ch = (unsigned char)*source++;
        if (ch == '.') destination[out++] = L',';
        else if (ch == '*') destination[out++] = L'×';
        else if (ch == '/') destination[out++] = L'÷';
        else if (ch == '-') destination[out++] = L'−';
        else destination[out++] = (wchar_t)ch;
    }
    destination[out] = L'\0';
}

static void pretty_number_to_ascii(const wchar_t *source, char *destination, size_t capacity) {
    size_t out = 0;
    while (*source && out + 1 < capacity) {
        wchar_t ch = *source++;
        if (ch == L',' || ch == L'.') destination[out++] = '.';
        else if (ch == L'−') destination[out++] = '-';
        else if ((ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'+' ||
                 ch == L'e' || ch == L'E') destination[out++] = (char)ch;
    }
    destination[out] = '\0';
}

static void pretty_expression_to_ascii(const wchar_t *source, char *destination, size_t capacity) {
    size_t out = 0;
    while (*source && out + 1 < capacity) {
        wchar_t ch = *source++;
        if (ch == L',' || ch == L'.') destination[out++] = '.';
        else if (ch == L'×') destination[out++] = '*';
        else if (ch == L'÷') destination[out++] = '/';
        else if (ch == L'−') destination[out++] = '-';
        else if (ch >= 32 && ch <= 126) destination[out++] = (char)ch;
    }
    destination[out] = '\0';
}

static int selected_result_text(wchar_t *destination, size_t capacity) {
    wchar_t current[160];
    int start, end;
    size_t count;
    if (!destination || capacity == 0) return 0;
    display_format_ascii_number(g_calc.display, current, _countof(current));
    sync_result_selection(current);
    start = g_result_selection_anchor;
    end = g_result_selection_end;
    if (start < 0 || end < 0 || start == end) return 0;
    if (start > end) {
        int swap = start;
        start = end;
        end = swap;
    }
    count = (size_t)(end - start);
    if (count >= capacity) count = capacity - 1;
    wmemcpy(destination, current + start, count);
    destination[count] = L'\0';
    return count > 0;
}

static void trim_wide(wchar_t *text_value) {
    wchar_t *start = text_value;
    size_t length;
    while (*start == L' ' || *start == L'\t' || *start == L'\r' || *start == L'\n') ++start;
    if (start != text_value) memmove(text_value, start, (wcslen(start) + 1) * sizeof(wchar_t));
    length = wcslen(text_value);
    while (length && (text_value[length - 1] == L' ' || text_value[length - 1] == L'\t' ||
                      text_value[length - 1] == L'\r' || text_value[length - 1] == L'\n')) {
        text_value[--length] = L'\0';
    }
}

static int is_pretty_number(const wchar_t *text_value) {
    int has_digit = 0;
    while (*text_value) {
        wchar_t character = *text_value++;
        if (character >= L'0' && character <= L'9') has_digit = 1;
        else if (character != L' ' && character != L',' && character != L'.' &&
                 character != L'−' && character != L'-' && character != L'+' &&
                 character != L'e' && character != L'E') return 0;
    }
    return has_digit;
}

static void add_history(const wchar_t *expression, const wchar_t *result) {
    HistoryEntry *entry;
    char result_ascii[160];
    wchar_t formatted_result[160];
    if (!expression || !*expression || !result || !*result) return;
    if (is_pretty_number(result)) {
        pretty_number_to_ascii(result, result_ascii, sizeof(result_ascii));
        display_format_ascii_number(result_ascii, formatted_result, _countof(formatted_result));
    } else {
        wcsncpy(formatted_result, result, _countof(formatted_result) - 1);
        formatted_result[_countof(formatted_result) - 1] = L'\0';
    }
    if (g_history_count > 0 &&
        wcscmp(g_history[g_history_count - 1].expression, expression) == 0 &&
        wcscmp(g_history[g_history_count - 1].result, formatted_result) == 0) return;
    if (g_history_count == HISTORY_CAPACITY) {
        memmove(&g_history[0], &g_history[1], sizeof(g_history[0]) * (HISTORY_CAPACITY - 1));
        --g_history_count;
    }
    entry = &g_history[g_history_count++];
    wcsncpy(entry->expression, expression, _countof(entry->expression) - 1);
    entry->expression[_countof(entry->expression) - 1] = L'\0';
    wcsncpy(entry->result, formatted_result, _countof(entry->result) - 1);
    entry->result[_countof(entry->result) - 1] = L'\0';
    entry->scroll_x = 0;
    trim_wide(entry->expression);
    trim_wide(entry->result);
    g_history_scroll = 0;
}

static int wide_to_utf8(const wchar_t *text_value, char **buffer) {
    int bytes = WideCharToMultiByte(CP_UTF8, 0, text_value, -1, NULL, 0, NULL, NULL);
    if (bytes <= 0) return 0;
    *buffer = (char *)malloc((size_t)bytes);
    if (!*buffer) return 0;
    WideCharToMultiByte(CP_UTF8, 0, text_value, -1, *buffer, bytes, NULL, NULL);
    return bytes;
}

static void save_history(void) {
    FILE *file;
    int index;
    file = _wfopen(g_history_path, L"wb");
    if (!file) return;
    /* Keep the original AHK logger's human-readable format: newest first,
       expression on one line, result on the next, blank line between entries. */
    for (index = g_history_count - 1; index >= 0; --index) {
        char *expr_utf8 = NULL;
        char *result_utf8 = NULL;
        if (wide_to_utf8(g_history[index].expression, &expr_utf8) &&
            wide_to_utf8(g_history[index].result, &result_utf8)) {
            fprintf(file, "%s\r\n%s\r\n\r\n", expr_utf8, result_utf8);
        }
        free(expr_utf8);
        free(result_utf8);
    }
    fclose(file);
}

static int parse_history_line(wchar_t *line) {
    wchar_t *separator;
    wchar_t expression[2048];
    wchar_t result[160];
    trim_wide(line);
    if (!*line || *line == L'#' || *line == L';') return 0;
    separator = wcschr(line, L'\t');
    if (separator) {
        *separator++ = L'\0';
        wcsncpy(expression, line, _countof(expression) - 1);
        expression[_countof(expression) - 1] = L'\0';
        wcsncpy(result, separator, _countof(result) - 1);
        result[_countof(result) - 1] = L'\0';
    } else {
        separator = wcsrchr(line, L'=');
        if (!separator || !separator[1]) return 0;
        *separator = L'\0';
        _snwprintf(expression, _countof(expression), L"%ls =", line);
        expression[_countof(expression) - 1] = L'\0';
        wcsncpy(result, separator + 1, _countof(result) - 1);
        result[_countof(result) - 1] = L'\0';
    }
    trim_wide(expression);
    trim_wide(result);
    if (!*expression || !*result) return 0;
    add_history(expression, result);
    return 1;
}

static int load_history_file(const wchar_t *path) {
    FILE *file;
    long size;
    char *bytes;
    wchar_t *wide;
    int wide_length;
    wchar_t *cursor;
    wchar_t pending_expression[2048] = L"";
    int imported = 0;
    int paired_entries = 0;
    int before = g_history_count;
    file = _wfopen(path, L"rb");
    if (!file) return 0;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0 || size > 8 * 1024 * 1024) {
        fclose(file);
        return 0;
    }
    bytes = (char *)malloc((size_t)size + 1);
    if (!bytes) {
        fclose(file);
        return 0;
    }
    size = (long)fread(bytes, 1, (size_t)size, file);
    fclose(file);
    bytes[size] = '\0';
    wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, (int)size, NULL, 0);
    if (!wide_length) wide_length = MultiByteToWideChar(CP_ACP, 0, bytes, (int)size, NULL, 0);
    if (!wide_length) {
        free(bytes);
        return 0;
    }
    wide = (wchar_t *)calloc((size_t)wide_length + 1, sizeof(wchar_t));
    if (!wide) {
        free(bytes);
        return 0;
    }
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, (int)size, wide, wide_length))
        MultiByteToWideChar(CP_ACP, 0, bytes, (int)size, wide, wide_length);
    free(bytes);
    cursor = wide;
    if (*cursor == 0xFEFF) ++cursor;
    while (*cursor) {
        wchar_t *end = wcspbrk(cursor, L"\r\n");
        wchar_t *next = NULL;
        if (end) {
            *end = L'\0';
            next = end + 1;
        } else {
            end = cursor + wcslen(cursor);
        }
        trim_wide(cursor);
        if (*cursor) {
            size_t length = wcslen(cursor);
            if (pending_expression[0]) {
                add_history(pending_expression, cursor);
                pending_expression[0] = L'\0';
                ++imported;
                ++paired_entries;
            } else if (cursor[length - 1] == L'=' && !wcschr(cursor, L'\t')) {
                wcsncpy(pending_expression, cursor, _countof(pending_expression) - 1);
                pending_expression[_countof(pending_expression) - 1] = L'\0';
            } else {
                imported += parse_history_line(cursor);
            }
        }
        if (!next) break;
        cursor = next;
        if (*cursor == L'\n' || *cursor == L'\r') ++cursor;
    }
    free(wide);
    /* The original AHK file is newest-first; the UI stores oldest-first so it
       can append efficiently and display from the end. */
    if (paired_entries > 0) {
        int left = before;
        int right = g_history_count - 1;
        while (left < right) {
            HistoryEntry swap = g_history[left];
            g_history[left] = g_history[right];
            g_history[right] = swap;
            ++left;
            --right;
        }
    }
    return imported;
}

static void build_history_path(void) {
    static const wchar_t filename[] = L"Windows Calculator Saved History.txt";
    wchar_t documents[MAX_PATH];
    wchar_t module_path[PATH_CAPACITY];
    wchar_t *slash;
    int written;
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT,
                                   documents))) {
        size_t length = wcslen(documents);
        written = _snwprintf(g_history_path, _countof(g_history_path), L"%ls%ls%ls",
                             documents,
                             length && documents[length - 1] == L'\\' ? L"" : L"\\",
                             filename);
        if (written >= 0 && (size_t)written < _countof(g_history_path)) return;
    }
    module_path[0] = L'\0';
    if (GetModuleFileNameW(NULL, module_path, _countof(module_path)) > 0) {
        slash = wcsrchr(module_path, L'\\');
        if (slash) *slash = L'\0';
        written = _snwprintf(g_history_path, _countof(g_history_path), L"%ls%ls%ls",
                             module_path, slash ? L"\\" : L"", filename);
        if (written >= 0 && (size_t)written < _countof(g_history_path)) return;
    }
    wcsncpy(g_history_path, filename, _countof(g_history_path) - 1);
    g_history_path[_countof(g_history_path) - 1] = L'\0';
}

static void try_legacy_history(void) {
    static const wchar_t *names[] = {L"CalculatorHistory.txt", L"history.txt", L"calc_history.txt"};
    wchar_t candidate[PATH_CAPACITY];
    wchar_t *slash;
    int i;
    if (g_history_count) return;
    for (i = 0; i < (int)_countof(names); ++i) {
        wcsncpy(candidate, g_history_path, _countof(candidate) - 1);
        candidate[_countof(candidate) - 1] = L'\0';
        slash = wcsrchr(candidate, L'\\');
        if (slash) {
            size_t remaining = _countof(candidate) - (size_t)(slash + 1 - candidate);
            wcsncpy(slash + 1, names[i], remaining - 1);
            slash[remaining] = L'\0';
        }
        if (_wcsicmp(candidate, g_history_path) != 0 && load_history_file(candidate)) {
            save_history();
            return;
        }
    }
}

static void record_ascii_history(const char *expression, const char *result, int continue_session) {
    wchar_t expression_wide[2048];
    wchar_t result_wide[160];
    ascii_to_pretty(expression, expression_wide, _countof(expression_wide));
    display_format_ascii_number(result, result_wide, _countof(result_wide));
    if (continue_session && g_calculation_history_index >= 0 &&
        g_calculation_history_index < g_history_count) {
        HistoryEntry *entry = &g_history[g_calculation_history_index];
        wcsncpy(entry->expression, expression_wide, _countof(entry->expression) - 1);
        entry->expression[_countof(entry->expression) - 1] = L'\0';
        wcsncpy(entry->result, result_wide, _countof(entry->result) - 1);
        entry->result[_countof(entry->result) - 1] = L'\0';
        trim_wide(entry->expression);
        trim_wide(entry->result);
        entry->scroll_x = 0;
    } else {
        add_history(expression_wide, result_wide);
        if (continue_session) g_calculation_history_index = g_history_count - 1;
    }
    save_history();
}

static void perform_binary_operator(char operation) {
    int completed_operation = g_calc.pending_operator != 0 && !g_calc.new_input;
    calc_set_operator(&g_calc, operation);
    if (completed_operation && !g_calc.error) {
        char checkpoint[2048];
        size_t length;
        snprintf(checkpoint, sizeof(checkpoint), "%s", g_calc.chain);
        length = strlen(checkpoint);
        while (length > 0 && checkpoint[length - 1] == ' ') --length;
        if (length > 0 && (checkpoint[length - 1] == '+' || checkpoint[length - 1] == '-' ||
                           checkpoint[length - 1] == '*' || checkpoint[length - 1] == '/'))
            --length;
        while (length > 0 && checkpoint[length - 1] == ' ') --length;
        checkpoint[length] = '\0';
        if (length + 3 < sizeof(checkpoint)) strcat(checkpoint, " =");
        record_ascii_history(checkpoint, g_calc.display, 1);
    }
}

static void copy_display_to_clipboard(HWND window) {
    wchar_t pretty[128];
    HGLOBAL memory;
    wchar_t *target;
    if (!selected_result_text(pretty, _countof(pretty)))
        display_format_ascii_number(g_calc.display, pretty, _countof(pretty));
    if (!OpenClipboard(window)) return;
    EmptyClipboard();
    memory = GlobalAlloc(GMEM_MOVEABLE, (wcslen(pretty) + 1) * sizeof(wchar_t));
    if (memory) {
        target = (wchar_t *)GlobalLock(memory);
        if (target) {
            wcscpy(target, pretty);
            GlobalUnlock(memory);
            SetClipboardData(CF_UNICODETEXT, memory);
            memory = NULL;
        }
    }
    if (memory) GlobalFree(memory);
    CloseClipboard();
}

static void show_result_context_menu(HWND window, POINT screen_point) {
    HMENU menu = CreatePopupMenu();
    UINT command;
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_COPY_RESULT, L"Copy");
    command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             screen_point.x, screen_point.y, 0, window, NULL);
    DestroyMenu(menu);
    if (command == ID_COPY_RESULT) copy_display_to_clipboard(window);
}

static void paste_from_clipboard(HWND window) {
    HANDLE data;
    const wchar_t *text_value;
    char ascii[160];
    char *end;
    double value;
    if (!OpenClipboard(window)) return;
    data = GetClipboardData(CF_UNICODETEXT);
    if (!data) {
        CloseClipboard();
        return;
    }
    text_value = (const wchar_t *)GlobalLock(data);
    if (text_value) {
        pretty_number_to_ascii(text_value, ascii, sizeof(ascii));
        value = strtod(ascii, &end);
        if (end != ascii && *end == '\0' && isfinite(value)) {
            if (!g_calc.pending_operator) g_calculation_history_index = -1;
            calc_set_value(&g_calc, value);
        }
        GlobalUnlock(data);
    }
    CloseClipboard();
}

static void draw_hamburger(HDC dc, RECT rect, COLORREF color) {
    int center_y = (rect.top + rect.bottom) / 2;
    HPEN pen = CreatePen(PS_SOLID, scale_px(1), color);
    HGDIOBJ old = SelectObject(dc, pen);
    int i;
    for (i = -1; i <= 1; ++i) {
        int y = center_y + i * scale_px(5);
        MoveToEx(dc, rect.left + scale_px(12), y, NULL);
        LineTo(dc, rect.right - scale_px(9), y);
    }
    SelectObject(dc, old);
    DeleteObject(pen);
}

static void draw_history_icon(HDC dc, RECT rect, COLORREF color) {
    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;
    int radius = scale_px(8);
    HPEN pen = CreatePen(PS_SOLID, scale_px(1), color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Arc(dc, cx - radius, cy - radius, cx + radius, cy + radius,
        cx - radius, cy, cx, cy - radius);
    MoveToEx(dc, cx - radius, cy, NULL);
    LineTo(dc, cx - radius - scale_px(4), cy - scale_px(3));
    MoveToEx(dc, cx - radius, cy, NULL);
    LineTo(dc, cx - radius + scale_px(1), cy - scale_px(5));
    MoveToEx(dc, cx, cy - scale_px(5), NULL);
    LineTo(dc, cx, cy);
    LineTo(dc, cx + scale_px(4), cy + scale_px(2));
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static void draw_trash_icon(HDC dc, RECT rect, COLORREF color) {
    int cx = (rect.left + rect.right) / 2;
    int top = rect.top + scale_px(9);
    int bottom = rect.bottom - scale_px(8);
    HPEN pen = CreatePen(PS_SOLID, scale_px(2), color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    MoveToEx(dc, cx - scale_px(7), top + scale_px(4), NULL);
    LineTo(dc, cx + scale_px(7), top + scale_px(4));
    MoveToEx(dc, cx - scale_px(3), top + scale_px(1), NULL);
    LineTo(dc, cx + scale_px(3), top + scale_px(1));
    Rectangle(dc, cx - scale_px(6), top + scale_px(7),
              cx + scale_px(6), bottom);
    MoveToEx(dc, cx - scale_px(2), top + scale_px(10), NULL);
    LineTo(dc, cx - scale_px(2), bottom - scale_px(3));
    MoveToEx(dc, cx + scale_px(2), top + scale_px(10), NULL);
    LineTo(dc, cx + scale_px(2), bottom - scale_px(3));
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static void draw_grid_button(HDC dc, int index, int width, int height) {
    int id = ID_GRID_BASE + index;
    RECT rect = grid_rect(index, width, height);
    COLORREF fill = g_buttons[index].kind == 1 ? COLOR_NUMBER : COLOR_BUTTON;
    COLORREF text_color = COLOR_TEXT;
    if (g_buttons[index].kind == 2) {
        fill = id == g_hot_id ? COLOR_EQUAL_HOVER : COLOR_EQUAL;
        text_color = RGB(24, 35, 40);
    } else if (id == g_pressed_id) fill = COLOR_PRESSED;
    else if (id == g_hot_id) fill = COLOR_HOVER;
    if (id == g_pressed_id && g_buttons[index].kind == 2) fill = RGB(135, 186, 209);
    rounded_rect(dc, &rect, fill, scale_px(5));
    draw_text_color(dc, g_buttons[index].label, rect, g_font_normal, text_color,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void draw_memory_row(HDC dc, int width, int height) {
    int i;
    for (i = 0; i < 6; ++i) {
        int id = ID_MEMORY_BASE + i;
        RECT rect = memory_rect(i, width, height);
        COLORREF color = ((i == 0 || i == 1 || i == 5) && !g_calc.has_memory)
                             ? COLOR_DISABLED : COLOR_TEXT;
        if (id == g_hot_id && color != COLOR_DISABLED) rounded_rect(dc, &rect, COLOR_BUTTON, scale_px(4));
        draw_text_color(dc, g_memory_labels[i], rect, g_font_small, color,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void draw_memory_popup(HDC dc, int width, int height) {
    RECT rect;
    wchar_t value[128];
    RECT title_rect, value_rect;
    int bottom = grid_top(height) - scale_px(3);
    rect.left = width - scale_px(230);
    if (rect.left < scale_px(8)) rect.left = scale_px(8);
    rect.right = width - scale_px(6);
    rect.bottom = bottom;
    rect.top = bottom - scale_px(92);
    rounded_rect(dc, &rect, RGB(43, 48, 50), scale_px(7));
    title_rect = rect;
    title_rect.left += scale_px(14);
    title_rect.top += scale_px(9);
    title_rect.bottom = title_rect.top + scale_px(25);
    draw_text_color(dc, L"Memory", title_rect, g_font_small, COLOR_MUTED,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    value_rect = rect;
    value_rect.left += scale_px(14);
    value_rect.right -= scale_px(14);
    value_rect.top += scale_px(34);
    if (g_calc.has_memory) {
        char raw[96];
        snprintf(raw, sizeof(raw), "%.15g", g_calc.memory);
        display_format_ascii_number(raw, value, _countof(value));
    } else {
        wcscpy(value, L"Nothing saved in memory");
    }
    draw_text_color(dc, value, value_rect, g_calc.has_memory ? g_font_history_result : g_font_normal,
                    g_calc.has_memory ? COLOR_TEXT : COLOR_MUTED,
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void draw_scrollable_expression(HDC dc, const wchar_t *text_value, RECT viewport,
                                       RECT track, HFONT font, COLORREF color, int id,
                                       int *scroll_x, int *scroll_max, RECT *thumb) {
    SIZE text_size = {0, 0};
    int viewport_width = viewport.right - viewport.left;
    int track_width = track.right - track.left;
    int thumb_width, travel, position;
    int saved_dc;
    HGDIOBJ old_font;
    RECT track_line;
    SetRectEmpty(thumb);
    *scroll_max = 0;
    if (!text_value || !*text_value || viewport_width <= 0) return;
    old_font = SelectObject(dc, font);
    GetTextExtentPoint32W(dc, text_value, (int)wcslen(text_value), &text_size);
    *scroll_max = text_size.cx > viewport_width ? text_size.cx - viewport_width : 0;
    if (*scroll_x < 0) *scroll_x = 0;
    if (*scroll_x > *scroll_max) *scroll_x = *scroll_max;
    saved_dc = SaveDC(dc);
    IntersectClipRect(dc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, viewport.right - text_size.cx + *scroll_x,
             viewport.top + ((viewport.bottom - viewport.top) - text_size.cy) / 2,
             text_value, (int)wcslen(text_value));
    RestoreDC(dc, saved_dc);
    SelectObject(dc, old_font);
    if (*scroll_max <= 0 || track_width <= 0) return;
    track_line = track;
    track_line.top += scale_px(2);
    track_line.bottom -= scale_px(2);
    rounded_rect(dc, &track_line, RGB(54, 54, 54), scale_px(3));
    thumb_width = (viewport_width * track_width) / text_size.cx;
    if (thumb_width < scale_px(24)) thumb_width = scale_px(24);
    if (thumb_width > track_width) thumb_width = track_width;
    travel = track_width - thumb_width;
    position = *scroll_max ? ((*scroll_max - *scroll_x) * travel) / *scroll_max : travel;
    thumb->left = track.left + position;
    thumb->right = thumb->left + thumb_width;
    thumb->top = track.top;
    thumb->bottom = track.bottom;
    rounded_rect(dc, thumb,
                 (g_hot_id == id || g_pressed_id == id || g_scroll_drag_id == id)
                     ? COLOR_TEXT : COLOR_MUTED,
                 scale_px(5));
}

static int history_visible_count(int height) {
    int available = height - scale_px(103) - scale_px(58);
    int item_height = scale_px(78);
    int visible = available > 0 ? available / item_height : 0;
    return visible > MAX_VISIBLE_HISTORY ? MAX_VISIBLE_HISTORY : visible;
}

static RECT history_wipe_rect(int width, int height) {
    int button_width = scale_px(160);
    RECT rect = {(width - button_width) / 2, height - scale_px(50),
                 (width + button_width) / 2, height - scale_px(9)};
    return rect;
}

static RECT history_item_rect(int row, int width) {
    RECT rect = {scale_px(10), scale_px(103) + row * scale_px(78),
                 width - scale_px(10), scale_px(103) + (row + 1) * scale_px(78) - scale_px(3)};
    return rect;
}

static void draw_history_panel(HDC dc, int width, int height) {
    RECT panel = {0, scale_px(50), width, height};
    RECT heading = {scale_px(14), scale_px(60), width - scale_px(14), scale_px(99)};
    int visible = history_visible_count(height);
    int row;
    RECT wipe_rect = history_wipe_rect(width, height);
    fill_rect_color(dc, &panel, COLOR_PANEL);
    draw_text_color(dc, L"History", heading, g_font_title, COLOR_TEXT,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    for (row = 0; row < MAX_VISIBLE_HISTORY; ++row) {
        g_history_scroll_entry[row] = -1;
        g_history_scroll_maximum[row] = 0;
        SetRectEmpty(&g_history_scroll_track[row]);
        SetRectEmpty(&g_history_scroll_thumb[row]);
    }
    if (!g_history_count) {
        RECT empty = {scale_px(14), scale_px(108), width - scale_px(14), scale_px(180)};
        draw_text_color(dc, L"There's no history yet.", empty, g_font_normal, COLOR_MUTED,
                        DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
    for (row = 0; row < visible; ++row) {
        int index = g_history_count - 1 - g_history_scroll - row;
        RECT item;
        RECT expression_rect, scrollbar_rect, result_rect;
        int id = ID_HISTORY_ITEM_BASE + row;
        if (index < 0) break;
        item = history_item_rect(row, width);
        if (id == g_hot_id || id == g_pressed_id) rounded_rect(dc, &item, COLOR_BUTTON, scale_px(5));
        expression_rect = item;
        expression_rect.left += scale_px(8);
        expression_rect.right -= scale_px(8);
        expression_rect.top += scale_px(3);
        expression_rect.bottom = expression_rect.top + scale_px(22);
        scrollbar_rect = expression_rect;
        scrollbar_rect.top = expression_rect.bottom;
        scrollbar_rect.bottom = scrollbar_rect.top + scale_px(7);
        result_rect = item;
        result_rect.left += scale_px(8);
        result_rect.right -= scale_px(8);
        result_rect.top += scale_px(34);
        result_rect.bottom -= scale_px(3);
        g_history_scroll_entry[row] = index;
        g_history_scroll_track[row] = scrollbar_rect;
        draw_scrollable_expression(dc, g_history[index].expression, expression_rect,
                                   scrollbar_rect, g_font_expression, COLOR_MUTED,
                                   ID_HISTORY_SCROLLBAR_BASE + row,
                                   &g_history[index].scroll_x,
                                   &g_history_scroll_maximum[row],
                                   &g_history_scroll_thumb[row]);
        draw_auto_fit_text(dc, g_history[index].result, result_rect, g_font_history_result,
                           18, 8, FW_SEMIBOLD, COLOR_TEXT);
    }
    if (g_hot_id == ID_CLEAR_HISTORY || g_pressed_id == ID_CLEAR_HISTORY)
        rounded_rect(dc, &wipe_rect, RGB(67, 39, 39), scale_px(5));
    {
        RECT label_rect = wipe_rect;
        RECT icon_rect = wipe_rect;
        label_rect.right -= scale_px(30);
        label_rect.left += scale_px(10);
        icon_rect.left = wipe_rect.right - scale_px(34);
        icon_rect.right = wipe_rect.right - scale_px(8);
        draw_text_color(dc, L"Wipe history", label_rect, g_font_normal, COLOR_DANGER,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        draw_trash_icon(dc, icon_rect, COLOR_DANGER);
    }
}

static void draw_nav_panel(HDC dc, int width, int height) {
    int panel_width = scale_px(246);
    RECT panel = {0, scale_px(50), panel_width < width ? panel_width : width, height};
    RECT caption = {scale_px(18), scale_px(62), panel.right - scale_px(10), scale_px(102)};
    RECT standard = {scale_px(6), scale_px(110), panel.right - scale_px(6), scale_px(158)};
    RECT changelog = {scale_px(6), scale_px(166), panel.right - scale_px(6), scale_px(214)};
    RECT updates = {scale_px(6), scale_px(222), panel.right - scale_px(6), scale_px(270)};
    RECT version = {scale_px(18), scale_px(272), panel.right - scale_px(12), scale_px(308)};
    COLORREF standard_fill = g_changelog_open ? COLOR_BUTTON : COLOR_PRESSED;
    COLORREF changelog_fill = g_changelog_open ? COLOR_PRESSED : COLOR_BUTTON;
    fill_rect_color(dc, &panel, RGB(37, 42, 43));
    draw_text_color(dc, L"Calculator", caption, g_font_title, COLOR_TEXT,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (g_hot_id == ID_NAV_STANDARD) standard_fill = COLOR_HOVER;
    rounded_rect(dc, &standard, standard_fill, scale_px(4));
    standard.left += scale_px(14);
    draw_text_color(dc, L"▣   Standard", standard, g_font_normal, COLOR_TEXT,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (g_hot_id == ID_NAV_CHANGELOG) changelog_fill = COLOR_HOVER;
    rounded_rect(dc, &changelog, changelog_fill, scale_px(4));
    changelog.left += scale_px(14);
    draw_text_color(dc, L"▤   Changelog", changelog, g_font_normal, COLOR_TEXT,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    rounded_rect(dc, &updates, g_hot_id == ID_CHECK_UPDATES ? COLOR_HOVER : COLOR_BUTTON,
                 scale_px(4));
    updates.left += scale_px(14);
    draw_text_color(dc, L"↻   Check for updates", updates, g_font_normal, COLOR_TEXT,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text_color(dc, L"Version " PERSISTENT_CALCULATOR_VERSION_W, version,
                    g_font_small, COLOR_MUTED,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static int changelog_release_count(void) {
    int count = (int)_countof(g_changelog_releases);
    return count < CHANGELOG_MAX_RELEASES ? count : CHANGELOG_MAX_RELEASES;
}

static RECT changelog_tab_rect(int index, int width) {
    int count = changelog_release_count();
    int margin = scale_px(8);
    int gap = scale_px(4);
    int available = width - margin * 2 - gap * (count - 1);
    RECT rect = {
        margin + (available * index) / count + gap * index,
        scale_px(62),
        margin + (available * (index + 1)) / count + gap * index,
        scale_px(98)
    };
    return rect;
}

static int changelog_text_height(HDC dc, const wchar_t *text_value, int width) {
    RECT measure = {0, 0, width, 0};
    HGDIOBJ old_font = SelectObject(dc, g_font_normal);
    DrawTextW(dc, text_value, -1, &measure,
              DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(dc, old_font);
    return measure.bottom > scale_px(22) ? measure.bottom : scale_px(22);
}

static void draw_changelog_panel(HDC dc, int width, int height) {
    int count = changelog_release_count();
    int content_top = scale_px(116);
    int content_bottom = height - scale_px(26);
    int viewport_height = content_bottom - content_top;
    int body_width = width - scale_px(64);
    int content_height = scale_px(58);
    int index;
    int y;
    int saved_dc;
    const ChangelogRelease *release;
    RECT panel = {0, scale_px(50), width, height};
    RECT divider = {scale_px(10), scale_px(105), width - scale_px(10), scale_px(106)};
    RECT hint = {scale_px(10), height - scale_px(24), width - scale_px(10), height};
    if (g_changelog_selected < 0 || g_changelog_selected >= count)
        g_changelog_selected = 0;
    release = &g_changelog_releases[g_changelog_selected];

    for (index = 0; index < release->item_count; ++index)
        content_height += changelog_text_height(dc, release->items[index], body_width) + scale_px(18);
    content_height += scale_px(14);
    g_changelog_scroll_max = content_height > viewport_height
                                 ? content_height - viewport_height : 0;
    if (g_changelog_scroll < 0) g_changelog_scroll = 0;
    if (g_changelog_scroll > g_changelog_scroll_max)
        g_changelog_scroll = g_changelog_scroll_max;

    fill_rect_color(dc, &panel, COLOR_PANEL);
    for (index = 0; index < count; ++index) {
        RECT tab = changelog_tab_rect(index, width);
        COLORREF fill = index == g_changelog_selected ? COLOR_PRESSED : COLOR_BUTTON;
        if (g_hot_id == ID_CHANGELOG_TAB_BASE + index) fill = COLOR_HOVER;
        rounded_rect(dc, &tab, fill, scale_px(5));
        draw_text_color(dc, g_changelog_releases[index].version, tab, g_font_small,
                        COLOR_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    fill_rect_color(dc, &divider, COLOR_HOVER);

    saved_dc = SaveDC(dc);
    IntersectClipRect(dc, 0, content_top, width, content_bottom);
    y = content_top - g_changelog_scroll;
    {
        RECT version_rect = {scale_px(18), y, width - scale_px(18), y + scale_px(30)};
        RECT date_rect = {scale_px(18), y + scale_px(30), width - scale_px(18), y + scale_px(52)};
        wchar_t heading[64];
        _snwprintf(heading, _countof(heading), L"Version %ls", release->version);
        heading[_countof(heading) - 1] = L'\0';
        draw_text_color(dc, heading, version_rect, g_font_title, COLOR_TEXT,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        draw_text_color(dc, release->date, date_rect, g_font_small, COLOR_MUTED,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    y += scale_px(62);
    for (index = 0; index < release->item_count; ++index) {
        int text_height = changelog_text_height(dc, release->items[index], body_width);
        RECT bullet = {scale_px(20), y, scale_px(36), y + text_height};
        RECT body = {scale_px(40), y, width - scale_px(24), y + text_height};
        draw_text_color(dc, L"•", bullet, g_font_title, COLOR_EQUAL,
                        DT_LEFT | DT_TOP | DT_SINGLELINE);
        draw_text_color(dc, release->items[index], body, g_font_normal, COLOR_TEXT,
                        DT_LEFT | DT_TOP | DT_WORDBREAK);
        y += text_height + scale_px(18);
    }
    RestoreDC(dc, saved_dc);

    if (g_changelog_scroll_max > 0) {
        int track_height = viewport_height;
        int thumb_height = (viewport_height * viewport_height) / content_height;
        int thumb_top;
        RECT track = {width - scale_px(7), content_top,
                      width - scale_px(3), content_bottom};
        RECT thumb;
        if (thumb_height < scale_px(30)) thumb_height = scale_px(30);
        thumb_top = content_top +
                    (g_changelog_scroll * (track_height - thumb_height)) /
                    g_changelog_scroll_max;
        thumb.left = track.left;
        thumb.right = track.right;
        thumb.top = thumb_top;
        thumb.bottom = thumb_top + thumb_height;
        rounded_rect(dc, &track, RGB(55, 55, 55), scale_px(2));
        rounded_rect(dc, &thumb, RGB(145, 149, 150), scale_px(2));
    }
    draw_text_color(dc, g_changelog_scroll_max > 0 ? L"Scroll for more" : L"Choose a version above",
                    hint, g_font_small, COLOR_MUTED,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void paint_window(HWND window, HDC target_dc) {
    RECT client;
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ old_bitmap;
    int width, height, index;
    RECT background, title, display_rect, expression_rect;
    wchar_t display[160];
    wchar_t expression[2048];
    GetClientRect(window, &client);
    width = client.right;
    height = client.bottom;
    dc = CreateCompatibleDC(target_dc);
    bitmap = CreateCompatibleBitmap(target_dc, width, height);
    old_bitmap = SelectObject(dc, bitmap);
    background = client;
    fill_rect_color(dc, &background, CLR_BACKGROUND);

    draw_hamburger(dc, header_menu_rect(), g_hot_id == ID_MENU ? COLOR_TEXT : COLOR_MUTED);
    title.left = scale_px(47);
    title.top = scale_px(5);
    title.right = width - scale_px(54);
    title.bottom = scale_px(49);
    draw_text_color(dc, g_changelog_open ? L"Changelog" : L"Standard",
                    title, g_font_title, COLOR_TEXT,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (!g_changelog_open)
        draw_history_icon(dc, header_history_rect(width),
                          g_hot_id == ID_HISTORY ? COLOR_TEXT : COLOR_MUTED);

    if (g_changelog_open) {
        g_active_scroll_max = 0;
        draw_changelog_panel(dc, width, height);
    } else {
        expression_rect.left = scale_px(14);
        expression_rect.right = width - scale_px(13);
        expression_rect.top = scale_px(51);
        expression_rect.bottom = scale_px(74);
        ascii_to_pretty(g_calc.expression, expression, _countof(expression));
        if (wcscmp(expression, g_last_active_expression) != 0) {
            g_active_scroll_x = 0;
            wcsncpy(g_last_active_expression, expression, _countof(g_last_active_expression) - 1);
            g_last_active_expression[_countof(g_last_active_expression) - 1] = L'\0';
        }
        g_active_scroll_track.left = expression_rect.left;
        g_active_scroll_track.right = expression_rect.right;
        g_active_scroll_track.top = expression_rect.bottom;
        g_active_scroll_track.bottom = expression_rect.bottom + scale_px(7);
        draw_scrollable_expression(dc, expression, expression_rect, g_active_scroll_track,
                                   g_font_expression, COLOR_MUTED, ID_ACTIVE_SCROLLBAR,
                                   &g_active_scroll_x, &g_active_scroll_max,
                                   &g_active_scroll_thumb);
        display_rect = result_display_rect(width, height);
        display_format_ascii_number(g_calc.display, display, _countof(display));
        draw_selectable_display(dc, display, display_rect);

        draw_memory_row(dc, width, height);
        for (index = 0; index < 24; ++index) draw_grid_button(dc, index, width, height);
        if (g_memory_popup) draw_memory_popup(dc, width, height);
        if (g_history_open) draw_history_panel(dc, width, height);
    }
    if (g_nav_open) draw_nav_panel(dc, width, height);

    BitBlt(target_dc, 0, 0, width, height, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

static int point_in_rect(RECT rect, int x, int y) {
    POINT point = {x, y};
    return PtInRect(&rect, point);
}

static int result_selection_available(void) {
    return !g_history_open && !g_nav_open && !g_changelog_open;
}

static int result_character_from_x(HWND window, int mouse_x) {
    RECT client;
    RECT rect;
    wchar_t text_value[160];
    HDC dc;
    HFONT fitted_font = NULL;
    HFONT font;
    HGDIOBJ old_font;
    SIZE full_size = {0, 0};
    SIZE prefix_size = {0, 0};
    int text_x, previous_width = 0;
    int index, length;
    GetClientRect(window, &client);
    rect = result_display_rect(client.right, client.bottom);
    display_format_ascii_number(g_calc.display, text_value, _countof(text_value));
    length = (int)wcslen(text_value);
    dc = GetDC(window);
    if (!dc) return 0;
    font = get_auto_fit_font(dc, text_value, rect, g_font_display, 35, 8,
                             FW_SEMIBOLD, &fitted_font);
    old_font = SelectObject(dc, font);
    GetTextExtentPoint32W(dc, text_value, length, &full_size);
    text_x = rect.right - full_size.cx;
    if (mouse_x <= text_x) index = 0;
    else if (mouse_x >= text_x + full_size.cx) index = length;
    else {
        index = length;
        for (int current = 0; current < length; ++current) {
            GetTextExtentPoint32W(dc, text_value, current + 1, &prefix_size);
            if (mouse_x < text_x + (previous_width + prefix_size.cx) / 2) {
                index = current;
                break;
            }
            previous_width = prefix_size.cx;
        }
    }
    SelectObject(dc, old_font);
    if (fitted_font) DeleteObject(fitted_font);
    ReleaseDC(window, dc);
    return index;
}

static void begin_result_selection(HWND window, int mouse_x) {
    wchar_t text_value[160];
    display_format_ascii_number(g_calc.display, text_value, _countof(text_value));
    wcsncpy(g_result_selection_text, text_value, _countof(g_result_selection_text) - 1);
    g_result_selection_text[_countof(g_result_selection_text) - 1] = L'\0';
    g_result_selection_anchor = result_character_from_x(window, mouse_x);
    g_result_selection_end = g_result_selection_anchor;
    g_result_selecting = 1;
    g_pressed_id = -1;
    SetCapture(window);
    InvalidateRect(window, NULL, FALSE);
}

static void update_result_selection(HWND window, int mouse_x) {
    int next = result_character_from_x(window, mouse_x);
    if (next != g_result_selection_end) {
        g_result_selection_end = next;
        InvalidateRect(window, NULL, FALSE);
    }
}

static int get_scrollbar_info(int id, RECT *track, RECT *thumb, int **value, int *maximum) {
    if (id == ID_ACTIVE_SCROLLBAR && g_active_scroll_max > 0) {
        *track = g_active_scroll_track;
        *thumb = g_active_scroll_thumb;
        *value = &g_active_scroll_x;
        *maximum = g_active_scroll_max;
        return 1;
    }
    if (id >= ID_HISTORY_SCROLLBAR_BASE &&
        id < ID_HISTORY_SCROLLBAR_BASE + MAX_VISIBLE_HISTORY) {
        int row = id - ID_HISTORY_SCROLLBAR_BASE;
        int entry = g_history_scroll_entry[row];
        if (entry >= 0 && entry < g_history_count && g_history_scroll_maximum[row] > 0) {
            *track = g_history_scroll_track[row];
            *thumb = g_history_scroll_thumb[row];
            *value = &g_history[entry].scroll_x;
            *maximum = g_history_scroll_maximum[row];
            return 1;
        }
    }
    return 0;
}

static void begin_scroll_drag(HWND window, int id, int mouse_x) {
    RECT track, thumb;
    int *value;
    int maximum;
    int travel, position;
    if (!get_scrollbar_info(id, &track, &thumb, &value, &maximum)) return;
    travel = (track.right - track.left) - (thumb.right - thumb.left);
    if (!point_in_rect(thumb, mouse_x, (thumb.top + thumb.bottom) / 2) && travel > 0) {
        position = mouse_x - track.left - (thumb.right - thumb.left) / 2;
        if (position < 0) position = 0;
        if (position > travel) position = travel;
        *value = maximum - (position * maximum) / travel;
        g_scroll_drag_start_thumb_left = track.left + position;
    } else {
        g_scroll_drag_start_thumb_left = thumb.left;
    }
    g_scroll_drag_id = id;
    g_scroll_drag_start_x = mouse_x;
    g_pressed_id = id;
    SetCapture(window);
    InvalidateRect(window, NULL, FALSE);
}

static void update_scroll_drag(HWND window, int mouse_x) {
    RECT track, thumb;
    int *value;
    int maximum;
    int travel, position;
    if (!get_scrollbar_info(g_scroll_drag_id, &track, &thumb, &value, &maximum)) return;
    travel = (track.right - track.left) - (thumb.right - thumb.left);
    if (travel <= 0) return;
    position = g_scroll_drag_start_thumb_left + mouse_x - g_scroll_drag_start_x - track.left;
    if (position < 0) position = 0;
    if (position > travel) position = travel;
    *value = maximum - (position * maximum) / travel;
    InvalidateRect(window, NULL, FALSE);
}

static int hit_test(HWND window, int x, int y) {
    RECT client;
    int width, height, index;
    GetClientRect(window, &client);
    width = client.right;
    height = client.bottom;
    if (point_in_rect(header_menu_rect(), x, y)) return ID_MENU;
    if (!g_changelog_open && point_in_rect(header_history_rect(width), x, y)) return ID_HISTORY;
    if (g_nav_open) {
        RECT standard = {scale_px(6), scale_px(110),
                         (scale_px(246) < width ? scale_px(246) : width) - scale_px(6), scale_px(158)};
        RECT changelog = {scale_px(6), scale_px(166),
                          (scale_px(246) < width ? scale_px(246) : width) - scale_px(6), scale_px(214)};
        RECT updates = {scale_px(6), scale_px(222),
                        (scale_px(246) < width ? scale_px(246) : width) - scale_px(6), scale_px(270)};
        if (point_in_rect(standard, x, y)) return ID_NAV_STANDARD;
        if (point_in_rect(changelog, x, y)) return ID_NAV_CHANGELOG;
        if (point_in_rect(updates, x, y)) return ID_CHECK_UPDATES;
        return -1;
    }
    if (g_changelog_open) {
        int count = changelog_release_count();
        for (index = 0; index < count; ++index)
            if (point_in_rect(changelog_tab_rect(index, width), x, y))
                return ID_CHANGELOG_TAB_BASE + index;
        return -1;
    }
    if (g_history_open) {
        int visible = history_visible_count(height);
        if (point_in_rect(history_wipe_rect(width, height), x, y)) return ID_CLEAR_HISTORY;
        for (index = 0; index < visible; ++index) {
            if (g_history_count - 1 - g_history_scroll - index < 0) break;
            if (g_history_scroll_maximum[index] > 0 &&
                point_in_rect(g_history_scroll_track[index], x, y))
                return ID_HISTORY_SCROLLBAR_BASE + index;
            if (point_in_rect(history_item_rect(index, width), x, y)) return ID_HISTORY_ITEM_BASE + index;
        }
        return -1;
    }
    if (g_active_scroll_max > 0 && point_in_rect(g_active_scroll_track, x, y))
        return ID_ACTIVE_SCROLLBAR;
    if (g_memory_popup) {
        int bottom = grid_top(height) - scale_px(3);
        RECT popup = {width - scale_px(230), bottom - scale_px(92), width - scale_px(6), bottom};
        if (popup.left < scale_px(8)) popup.left = scale_px(8);
        if (point_in_rect(popup, x, y) && g_calc.has_memory) return ID_MEMORY_VALUE;
    }
    for (index = 0; index < 6; ++index) {
        if (point_in_rect(memory_rect(index, width, height), x, y)) {
            if ((index == 0 || index == 1 || index == 5) && !g_calc.has_memory) return -1;
            return ID_MEMORY_BASE + index;
        }
    }
    for (index = 0; index < 24; ++index)
        if (point_in_rect(grid_rect(index, width, height), x, y)) return ID_GRID_BASE + index;
    return -1;
}

static void recall_history_row(int row) {
    int index = g_history_count - 1 - g_history_scroll - row;
    char ascii[160];
    char expression_ascii[2048];
    char *end;
    double value;
    if (index < 0 || index >= g_history_count) return;
    pretty_number_to_ascii(g_history[index].result, ascii, sizeof(ascii));
    value = strtod(ascii, &end);
    if (end != ascii && *end == '\0' && isfinite(value)) {
        pretty_expression_to_ascii(g_history[index].expression, expression_ascii,
                                   sizeof(expression_ascii));
        calc_restore_history(&g_calc, value, expression_ascii);
        g_calculation_history_index = -1;
        g_active_scroll_x = 0;
        g_history_open = 0;
    }
}

static void activate_grid_button(int index) {
    char history_expression[2048];
    char history_result[96];
    switch (index) {
        case 0:
            if (g_calc.continuation_available) g_calculation_history_index = -1;
            calc_percent(&g_calc);
            break;
        case 1: calc_clear_entry(&g_calc); break;
        case 2: g_calculation_history_index = -1; calc_clear_all(&g_calc); break;
        case 3: calc_backspace(&g_calc); break;
        case 4:
            g_calculation_history_index = -1;
            if (calc_unary(&g_calc, 'r', history_expression, sizeof(history_expression),
                           history_result, sizeof(history_result)))
                record_ascii_history(history_expression, history_result, 0);
            break;
        case 5:
            g_calculation_history_index = -1;
            if (calc_unary(&g_calc, 's', history_expression, sizeof(history_expression),
                           history_result, sizeof(history_result)))
                record_ascii_history(history_expression, history_result, 0);
            break;
        case 6:
            g_calculation_history_index = -1;
            if (calc_unary(&g_calc, 'q', history_expression, sizeof(history_expression),
                           history_result, sizeof(history_result)))
                record_ascii_history(history_expression, history_result, 0);
            break;
        case 7: perform_binary_operator('/'); break;
        case 8: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 7); break;
        case 9: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 8); break;
        case 10: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 9); break;
        case 11: perform_binary_operator('*'); break;
        case 12: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 4); break;
        case 13: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 5); break;
        case 14: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 6); break;
        case 15: perform_binary_operator('-'); break;
        case 16: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 1); break;
        case 17: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 2); break;
        case 18: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 3); break;
        case 19: perform_binary_operator('+'); break;
        case 20:
            if (g_calc.continuation_available) {
                g_calculation_history_index = -1;
                g_calc.continuation_available = 0;
                g_calc.chain[0] = '\0';
                g_calc.expression[0] = '\0';
            }
            calc_toggle_sign(&g_calc);
            break;
        case 21: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_digit(&g_calc, 0); break;
        case 22: if (g_calc.continuation_available) g_calculation_history_index = -1; calc_decimal(&g_calc); break;
        case 23:
            if (calc_equals(&g_calc, history_expression, sizeof(history_expression),
                            history_result, sizeof(history_result)))
                record_ascii_history(history_expression, history_result, 1);
            break;
    }
}

static void activate_id(HWND window, int id) {
    if (id >= ID_GRID_BASE && id < ID_GRID_BASE + 24) {
        activate_grid_button(id - ID_GRID_BASE);
    } else if (id >= ID_MEMORY_BASE && id < ID_MEMORY_BASE + 6) {
        switch (id - ID_MEMORY_BASE) {
            case 0: calc_memory_clear(&g_calc); g_memory_popup = 0; break;
            case 1:
                if (!g_calc.pending_operator) g_calculation_history_index = -1;
                calc_memory_recall(&g_calc);
                break;
            case 2: calc_memory_add(&g_calc); break;
            case 3: calc_memory_subtract(&g_calc); break;
            case 4: calc_memory_store(&g_calc); break;
            case 5: g_memory_popup = !g_memory_popup; break;
        }
    } else if (id == ID_HISTORY) {
        if (g_changelog_open) return;
        g_history_open = !g_history_open;
        g_nav_open = 0;
        g_memory_popup = 0;
    } else if (id == ID_MENU) {
        g_nav_open = !g_nav_open;
        g_history_open = 0;
        g_memory_popup = 0;
    } else if (id == ID_NAV_STANDARD) {
        g_changelog_open = 0;
        g_history_open = 0;
        g_memory_popup = 0;
        g_nav_open = 0;
    } else if (id == ID_NAV_CHANGELOG) {
        clear_result_selection();
        g_changelog_open = 1;
        g_changelog_selected = 0;
        g_changelog_scroll = 0;
        g_history_open = 0;
        g_memory_popup = 0;
        g_nav_open = 0;
    } else if (id >= ID_CHANGELOG_TAB_BASE &&
               id < ID_CHANGELOG_TAB_BASE + changelog_release_count()) {
        g_changelog_selected = id - ID_CHANGELOG_TAB_BASE;
        g_changelog_scroll = 0;
    } else if (id == ID_CHECK_UPDATES) {
        g_nav_open = 0;
        updater_check_now(window);
    } else if (id == ID_CLEAR_HISTORY) {
        g_history_count = 0;
        g_history_scroll = 0;
        g_calculation_history_index = -1;
        save_history();
    } else if (id >= ID_HISTORY_ITEM_BASE && id < ID_HISTORY_ITEM_BASE + 100) {
        recall_history_row(id - ID_HISTORY_ITEM_BASE);
    } else if (id == ID_MEMORY_VALUE) {
        calc_memory_recall(&g_calc);
        g_memory_popup = 0;
    }
    InvalidateRect(window, NULL, FALSE);
}

static void keyboard_character(HWND window, wchar_t character) {
    if (character >= L'0' && character <= L'9') {
        if (g_calc.continuation_available) g_calculation_history_index = -1;
        calc_digit(&g_calc, (int)(character - L'0'));
    } else {
        switch (character) {
            case L'.': case L',':
                if (g_calc.continuation_available) g_calculation_history_index = -1;
                calc_decimal(&g_calc);
                break;
            case L'+': perform_binary_operator('+'); break;
            case L'-': perform_binary_operator('-'); break;
            case L'*': case L'x': case L'X': perform_binary_operator('*'); break;
            case L'/': perform_binary_operator('/'); break;
            case L'%':
                if (g_calc.continuation_available) g_calculation_history_index = -1;
                calc_percent(&g_calc);
                break;
            case L'=': activate_grid_button(23); break;
            case L'@': activate_grid_button(6); break;
            case L'q': case L'Q': activate_grid_button(5); break;
            case L'r': case L'R': activate_grid_button(4); break;
        }
    }
    InvalidateRect(window, NULL, FALSE);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            BOOL dark = TRUE;
            DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
            g_dpi = GetDpiForWindow(window);
            if (!g_dpi) g_dpi = 96;
            create_fonts();
            DragAcceptFiles(window, FALSE);
            return 0;
        }
        case WM_DPICHANGED: {
            RECT *suggested = (RECT *)lparam;
            g_dpi = HIWORD(wparam);
            create_fonts();
            SetWindowPos(window, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            updater_owner_resized(window);
            return 0;
        }
        case WM_SIZE:
            updater_owner_resized(window);
            InvalidateRect(window, NULL, FALSE);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO *info = (MINMAXINFO *)lparam;
            info->ptMinTrackSize.x = scale_px(322);
            info->ptMinTrackSize.y = scale_px(575);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window, &paint);
            paint_window(window, dc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lparam) == HTCLIENT && result_selection_available()) {
                POINT point;
                RECT client;
                RECT display;
                GetCursorPos(&point);
                ScreenToClient(window, &point);
                GetClientRect(window, &client);
                display = result_display_rect(client.right, client.bottom);
                if (point_in_rect(display, point.x, point.y)) {
                    SetCursor(LoadCursorW(NULL, IDC_IBEAM));
                    return TRUE;
                }
            }
            return DefWindowProcW(window, message, wparam, lparam);
        }
        case WM_MOUSEMOVE: {
            if (g_result_selecting) {
                update_result_selection(window, GET_X_LPARAM(lparam));
                return 0;
            }
            if (g_scroll_drag_id >= 0) {
                update_scroll_drag(window, GET_X_LPARAM(lparam));
                return 0;
            }
            int new_hot = hit_test(window, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, window, 0};
            TrackMouseEvent(&track);
            if (new_hot != g_hot_id) {
                g_hot_id = new_hot;
                InvalidateRect(window, NULL, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            g_hot_id = -1;
            InvalidateRect(window, NULL, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            RECT client;
            RECT display;
            int mouse_x = GET_X_LPARAM(lparam);
            int mouse_y = GET_Y_LPARAM(lparam);
            GetClientRect(window, &client);
            display = result_display_rect(client.right, client.bottom);
            if (result_selection_available() && point_in_rect(display, mouse_x, mouse_y)) {
                begin_result_selection(window, mouse_x);
                return 0;
            }
            clear_result_selection();
            g_pressed_id = hit_test(window, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            if (g_pressed_id == ID_ACTIVE_SCROLLBAR ||
                (g_pressed_id >= ID_HISTORY_SCROLLBAR_BASE &&
                 g_pressed_id < ID_HISTORY_SCROLLBAR_BASE + MAX_VISIBLE_HISTORY)) {
                begin_scroll_drag(window, g_pressed_id, GET_X_LPARAM(lparam));
                return 0;
            }
            if (g_pressed_id >= 0) SetCapture(window);
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_result_selecting) {
                update_result_selection(window, GET_X_LPARAM(lparam));
                g_result_selecting = 0;
                ReleaseCapture();
                InvalidateRect(window, NULL, FALSE);
                return 0;
            }
            if (g_scroll_drag_id >= 0) {
                g_scroll_drag_id = -1;
                g_pressed_id = -1;
                ReleaseCapture();
                InvalidateRect(window, NULL, FALSE);
                return 0;
            }
            int released = hit_test(window, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            int pressed = g_pressed_id;
            g_pressed_id = -1;
            ReleaseCapture();
            if (pressed >= 0 && pressed == released) activate_id(window, pressed);
            else InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        case WM_CONTEXTMENU: {
            POINT screen_point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            POINT client_point = screen_point;
            RECT client;
            RECT display;
            if (!result_selection_available())
                return DefWindowProcW(window, message, wparam, lparam);
            GetClientRect(window, &client);
            display = result_display_rect(client.right, client.bottom);
            if (screen_point.x == -1 && screen_point.y == -1) {
                client_point.x = display.right;
                client_point.y = (display.top + display.bottom) / 2;
                screen_point = client_point;
                ClientToScreen(window, &screen_point);
            } else {
                ScreenToClient(window, &client_point);
                if (!point_in_rect(display, client_point.x, client_point.y))
                    return DefWindowProcW(window, message, wparam, lparam);
            }
            show_result_context_menu(window, screen_point);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT point = {(short)LOWORD(lparam), (short)HIWORD(lparam)};
            int wheel_steps = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
            ScreenToClient(window, &point);
            if (updater_is_modal()) return 0;
            if (g_changelog_open && !g_nav_open) {
                g_changelog_scroll -= wheel_steps * scale_px(54);
                if (g_changelog_scroll < 0) g_changelog_scroll = 0;
                if (g_changelog_scroll > g_changelog_scroll_max)
                    g_changelog_scroll = g_changelog_scroll_max;
                InvalidateRect(window, NULL, FALSE);
            } else if (g_history_open && g_history_count) {
                RECT client;
                int visible, maximum;
                GetClientRect(window, &client);
                visible = history_visible_count(client.bottom);
                maximum = g_history_count > visible ? g_history_count - visible : 0;
                if (wheel_steps < 0 && g_history_scroll < maximum) ++g_history_scroll;
                if (wheel_steps > 0 && g_history_scroll > 0) --g_history_scroll;
                InvalidateRect(window, NULL, FALSE);
            } else if (!g_nav_open && g_active_scroll_max > 0) {
                RECT active_zone = g_active_scroll_track;
                active_zone.top = scale_px(50);
                if (point_in_rect(active_zone, point.x, point.y)) {
                    g_active_scroll_x += wheel_steps * scale_px(45);
                    if (g_active_scroll_x < 0) g_active_scroll_x = 0;
                    if (g_active_scroll_x > g_active_scroll_max)
                        g_active_scroll_x = g_active_scroll_max;
                    InvalidateRect(window, NULL, FALSE);
                }
            }
            return 0;
        }
        case WM_KEYDOWN: {
            int control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (updater_is_modal()) return 0;
            if (g_changelog_open) {
                switch (wparam) {
                    case VK_UP: g_changelog_scroll -= scale_px(36); break;
                    case VK_DOWN: g_changelog_scroll += scale_px(36); break;
                    case VK_PRIOR: g_changelog_scroll -= scale_px(180); break;
                    case VK_NEXT: g_changelog_scroll += scale_px(180); break;
                    case VK_HOME: g_changelog_scroll = 0; break;
                    case VK_END: g_changelog_scroll = g_changelog_scroll_max; break;
                    default: return 0;
                }
                if (g_changelog_scroll < 0) g_changelog_scroll = 0;
                if (g_changelog_scroll > g_changelog_scroll_max)
                    g_changelog_scroll = g_changelog_scroll_max;
                InvalidateRect(window, NULL, FALSE);
                return 0;
            }
            if (control) {
                switch (wparam) {
                    case 'C': copy_display_to_clipboard(window); return 0;
                    case 'V': paste_from_clipboard(window); InvalidateRect(window, NULL, FALSE); return 0;
                    case 'H': activate_id(window, ID_HISTORY); return 0;
                    case 'L': calc_memory_clear(&g_calc); break;
                    case 'R': calc_memory_recall(&g_calc); break;
                    case 'P': calc_memory_add(&g_calc); break;
                    case 'Q': calc_memory_subtract(&g_calc); break;
                    case 'M': calc_memory_store(&g_calc); break;
                    default: break;
                }
                InvalidateRect(window, NULL, FALSE);
                return 0;
            }
            switch (wparam) {
                case VK_RETURN: activate_grid_button(23); break;
                case VK_BACK: calc_backspace(&g_calc); break;
                case VK_ESCAPE:
                    g_calculation_history_index = -1;
                    calc_clear_all(&g_calc);
                    break;
                case VK_DELETE:
                    calc_clear_entry(&g_calc);
                    break;
                case VK_F9:
                    if (g_calc.continuation_available) {
                        g_calculation_history_index = -1;
                        g_calc.continuation_available = 0;
                        g_calc.chain[0] = '\0';
                        g_calc.expression[0] = '\0';
                    }
                    calc_toggle_sign(&g_calc);
                    break;
                default: return DefWindowProcW(window, message, wparam, lparam);
            }
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        case WM_CHAR:
            if (updater_is_modal()) return 0;
            if (g_changelog_open) return 0;
            if (wparam != VK_RETURN && wparam != VK_BACK && wparam != VK_ESCAPE)
                keyboard_character(window, (wchar_t)wparam);
            return 0;
        case WM_CLOSE:
            save_window_placement(window);
            DestroyWindow(window);
            return 0;
        case WM_ENDSESSION:
            if (wparam) save_window_placement(window);
            return 0;
        case WM_DESTROY:
            save_history();
            updater_shutdown();
            destroy_fonts();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, wchar_t *command_line, int show) {
    WNDCLASSEXW window_class;
    HWND window;
    MSG message;
    RECT desired = {0, 0, 322, 544};
    RECT saved_rect;
    HDC screen;
    int screen_dpi;
    int saved_maximized = 0;
    int has_saved_placement;
    int window_x = CW_USEDEFAULT;
    int window_y = CW_USEDEFAULT;
    int window_width;
    int window_height;
    (void)previous;
    (void)command_line;

    if (updater_apply_if_requested()) return 0;

    SetProcessDPIAware();
    calc_init(&g_calc);
    build_history_path();
    load_history_file(g_history_path);
    try_legacy_history();
    if (GetFileAttributesW(g_history_path) == INVALID_FILE_ATTRIBUTES) save_history();

    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (!window_class.hIcon) window_class.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    if (!window_class.hIconSm) window_class.hIconSm = LoadIconW(NULL, IDI_APPLICATION);
    window_class.hbrBackground = CreateSolidBrush(CLR_BACKGROUND);
    window_class.lpszClassName = APP_CLASS;
    if (!RegisterClassExW(&window_class)) return 1;

    screen = GetDC(NULL);
    screen_dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(NULL, screen);
    if (screen_dpi <= 0) screen_dpi = 96;
    desired.right = MulDiv(desired.right, screen_dpi, 96);
    desired.bottom = MulDiv(desired.bottom, screen_dpi, 96);
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    window_width = desired.right - desired.left;
    window_height = desired.bottom - desired.top;
    has_saved_placement = load_window_placement(&saved_rect, &saved_maximized);
    if (has_saved_placement) {
        window_x = saved_rect.left;
        window_y = saved_rect.top;
        window_width = saved_rect.right - saved_rect.left;
        window_height = saved_rect.bottom - saved_rect.top;
    }
    window = CreateWindowExW(0, APP_CLASS, APP_TITLE, WS_OVERLAPPEDWINDOW,
                             window_x, window_y, window_width, window_height,
                             NULL, NULL, instance, NULL);
    if (!window) return 2;
    ShowWindow(window, saved_maximized ? SW_SHOWMAXIMIZED : show);
    UpdateWindow(window);
    updater_initialize(window);
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
