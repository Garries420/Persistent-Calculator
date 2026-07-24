#ifndef PERSISTENT_CALCULATOR_EXTRAS_H
#define PERSISTENT_CALCULATOR_EXTRAS_H

#include <windows.h>
#include <stddef.h>

typedef enum ExtraMode {
    MODE_STANDARD = 0,
    MODE_SCIENTIFIC,
    MODE_PROGRAMMER,
    MODE_DATE,
    MODE_CURRENCY,
    MODE_VOLUME,
    MODE_LENGTH,
    MODE_WEIGHT,
    MODE_TEMPERATURE,
    MODE_ENERGY,
    MODE_AREA,
    MODE_SPEED,
    MODE_TIME,
    MODE_POWER,
    MODE_DATA,
    MODE_PRESSURE,
    MODE_ANGLE,
    MODE_COUNT
} ExtraMode;

#define EXTRA_ID_BASE 2000
#define EXTRA_CURRENCY_READY (WM_APP + 42)
#define EXTRA_ID_CURRENCY_SOURCE (EXTRA_ID_BASE + 410)

void extras_initialize(HWND owner);
void extras_shutdown(void);
void extras_set_mode(HWND owner, ExtraMode mode);
ExtraMode extras_mode(void);
const wchar_t *extras_mode_name(ExtraMode mode);

void extras_paint(HDC dc, int width, int height, UINT dpi, int hot_id, int pressed_id);
int extras_hit_test(int x, int y, int width, int height, UINT dpi);
void extras_activate(HWND owner, int id);
void extras_key_down(HWND owner, WPARAM key);
void extras_character(HWND owner, wchar_t character);
void extras_mouse_wheel(HWND owner, int wheel_steps);
void extras_currency_ready(HWND owner, LPARAM status);
int extras_text_hit_test(int x, int y, int width, int height, UINT dpi);
void extras_begin_text_selection(HWND owner, int x, int y,
                                 int width, int height, UINT dpi);
void extras_update_text_selection(HWND owner, int x,
                                  int width, int height, UINT dpi);
void extras_end_text_selection(HWND owner);
int extras_text_selection_dragging(void);
void extras_focus_text_at(HWND owner, int x, int y,
                          int width, int height, UINT dpi);
void extras_clear_text_selection(void);
int extras_copy_text(HWND owner, int width, int height, UINT dpi);
int extras_paste_text(HWND owner);

int extras_take_history(wchar_t *expression, size_t expression_capacity,
                        wchar_t *result, size_t result_capacity);

/* Pure helpers used by tests. */
double extras_convert_value(ExtraMode mode, int from_index, int to_index, double value);
int extras_unit_count(ExtraMode mode);
const wchar_t *extras_unit_name(ExtraMode mode, int index);

#endif
