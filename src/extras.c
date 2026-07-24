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

#include "extras.h"

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <winhttp.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include <wchar.h>

#define EXTRA_PATH_CAPACITY 32768
#define EXTRA_MAX_CURRENCIES 256
#define EXTRA_MAX_PICKER_ROWS 8

typedef struct UnitDef {
    const wchar_t *name;
    const wchar_t *symbol;
    double scale;
    double offset;
} UnitDef;

typedef struct CurrencyRate {
    wchar_t code[8];
    wchar_t location[64];
    wchar_t name[64];
    wchar_t symbol[16];
    wchar_t label[144];
    int symbol_prefix;
    double rate;
} CurrencyRate;

typedef struct ConverterState {
    char input[MODE_COUNT][96];
    int new_input[MODE_COUNT];
    int from_index[MODE_COUNT];
    int to_index[MODE_COUNT];
    int picker_open;
    int picker_target;
    int picker_scroll;
    wchar_t picker_search[64];
    ULONGLONG picker_search_tick;
} ConverterState;

typedef struct ScientificState {
    char display[96];
    double accumulator;
    double operand;
    char pending;
    int has_accumulator;
    int new_input;
    int error;
    int angle_mode;
    int inverse;
    int hyperbolic;
    int f_e;
    int popup;
    int group_active;
    double outer_accumulator;
    char outer_pending;
} ScientificState;

typedef struct ProgrammerState {
    uint64_t value;
    uint64_t accumulator;
    int base;
    int bits;
    int new_input;
    int has_accumulator;
    int pending;
    int error;
    int popup;
    int bit_keypad;
    int shift_mode;
    int carry;
    int group_active;
    uint64_t outer_accumulator;
    int outer_pending;
} ProgrammerState;

typedef struct DateState {
    SYSTEMTIME first;
    SYSTEMTIME second;
    int add_mode;
    int add_amount;
    int add_unit;
    int subtract;
    int calendar_target;
    int calendar_year;
    int calendar_month;
} DateState;

typedef struct ExtraTextSelection {
    int field;
    int anchor;
    int end;
    int dragging;
} ExtraTextSelection;

static const UnitDef UNITS_VOLUME[] = {
    {L"Millilitres", L"mL", 0.000001, 0.0},
    {L"Litres", L"L", 0.001, 0.0},
    {L"Cubic centimetres", L"cm³", 0.000001, 0.0},
    {L"Cubic metres", L"m³", 1.0, 0.0},
    {L"Cubic inches", L"in³", 0.000016387064, 0.0},
    {L"Cubic feet", L"ft³", 0.028316846592, 0.0},
    {L"US gallons", L"US gal", 0.003785411784, 0.0},
    {L"Imperial gallons", L"imp gal", 0.00454609, 0.0},
    {L"US fluid ounces", L"US fl oz", 0.0000295735295625, 0.0}
};

static const UnitDef UNITS_LENGTH[] = {
    {L"Nanometres", L"nm", 0.000000001, 0.0},
    {L"Micrometres", L"µm", 0.000001, 0.0},
    {L"Millimetres", L"mm", 0.001, 0.0},
    {L"Centimetres", L"cm", 0.01, 0.0},
    {L"Metres", L"m", 1.0, 0.0},
    {L"Kilometres", L"km", 1000.0, 0.0},
    {L"Inches", L"in", 0.0254, 0.0},
    {L"Feet", L"ft", 0.3048, 0.0},
    {L"Yards", L"yd", 0.9144, 0.0},
    {L"Miles", L"mi", 1609.344, 0.0},
    {L"Nautical miles", L"nmi", 1852.0, 0.0}
};

static const UnitDef UNITS_WEIGHT[] = {
    {L"Milligrams", L"mg", 0.000001, 0.0},
    {L"Grams", L"g", 0.001, 0.0},
    {L"Kilograms", L"kg", 1.0, 0.0},
    {L"Metric tonnes", L"t", 1000.0, 0.0},
    {L"Ounces", L"oz", 0.028349523125, 0.0},
    {L"Pounds", L"lb", 0.45359237, 0.0},
    {L"Stone", L"st", 6.35029318, 0.0}
};

static const UnitDef UNITS_TEMPERATURE[] = {
    {L"Celsius", L"°C", 1.0, 273.15},
    {L"Fahrenheit", L"°F", 5.0 / 9.0, 255.3722222222222},
    {L"Kelvin", L"K", 1.0, 0.0}
};

static const UnitDef UNITS_ENERGY[] = {
    {L"Joules", L"J", 1.0, 0.0},
    {L"Kilojoules", L"kJ", 1000.0, 0.0},
    {L"Calories", L"cal", 4.184, 0.0},
    {L"Kilocalories", L"kcal", 4184.0, 0.0},
    {L"Watt-hours", L"Wh", 3600.0, 0.0},
    {L"Kilowatt-hours", L"kWh", 3600000.0, 0.0},
    {L"British thermal units", L"BTU", 1055.05585262, 0.0},
    {L"Electronvolts", L"eV", 1.602176634e-19, 0.0}
};

static const UnitDef UNITS_AREA[] = {
    {L"Square millimetres", L"mm²", 0.000001, 0.0},
    {L"Square centimetres", L"cm²", 0.0001, 0.0},
    {L"Square metres", L"m²", 1.0, 0.0},
    {L"Square kilometres", L"km²", 1000000.0, 0.0},
    {L"Square inches", L"in²", 0.00064516, 0.0},
    {L"Square feet", L"ft²", 0.09290304, 0.0},
    {L"Square yards", L"yd²", 0.83612736, 0.0},
    {L"Acres", L"ac", 4046.8564224, 0.0},
    {L"Hectares", L"ha", 10000.0, 0.0},
    {L"Square miles", L"mi²", 2589988.110336, 0.0}
};

static const UnitDef UNITS_SPEED[] = {
    {L"Metres per second", L"m/s", 1.0, 0.0},
    {L"Kilometres per hour", L"km/h", 1.0 / 3.6, 0.0},
    {L"Miles per hour", L"mph", 0.44704, 0.0},
    {L"Knots", L"kn", 0.5144444444444444, 0.0},
    {L"Feet per second", L"ft/s", 0.3048, 0.0}
};

static const UnitDef UNITS_TIME[] = {
    {L"Microseconds", L"µs", 0.000001, 0.0},
    {L"Milliseconds", L"ms", 0.001, 0.0},
    {L"Seconds", L"s", 1.0, 0.0},
    {L"Minutes", L"min", 60.0, 0.0},
    {L"Hours", L"h", 3600.0, 0.0},
    {L"Days", L"d", 86400.0, 0.0},
    {L"Weeks", L"wk", 604800.0, 0.0},
    {L"Years (365 days)", L"yr", 31536000.0, 0.0}
};

static const UnitDef UNITS_POWER[] = {
    {L"Watts", L"W", 1.0, 0.0},
    {L"Kilowatts", L"kW", 1000.0, 0.0},
    {L"Megawatts", L"MW", 1000000.0, 0.0},
    {L"Metric horsepower", L"PS", 735.49875, 0.0},
    {L"Mechanical horsepower", L"hp", 745.6998715822702, 0.0},
    {L"BTU per hour", L"BTU/h", 0.2930710701722222, 0.0}
};

static const UnitDef UNITS_DATA[] = {
    {L"Bits", L"bit", 0.125, 0.0},
    {L"Bytes", L"B", 1.0, 0.0},
    {L"Kilobytes", L"KB", 1000.0, 0.0},
    {L"Kibibytes", L"KiB", 1024.0, 0.0},
    {L"Megabytes", L"MB", 1000000.0, 0.0},
    {L"Mebibytes", L"MiB", 1048576.0, 0.0},
    {L"Gigabytes", L"GB", 1000000000.0, 0.0},
    {L"Gibibytes", L"GiB", 1073741824.0, 0.0},
    {L"Terabytes", L"TB", 1000000000000.0, 0.0},
    {L"Tebibytes", L"TiB", 1099511627776.0, 0.0}
};

static const UnitDef UNITS_PRESSURE[] = {
    {L"Pascals", L"Pa", 1.0, 0.0},
    {L"Kilopascals", L"kPa", 1000.0, 0.0},
    {L"Megapascals", L"MPa", 1000000.0, 0.0},
    {L"Bars", L"bar", 100000.0, 0.0},
    {L"Millibars", L"mbar", 100.0, 0.0},
    {L"Atmospheres", L"atm", 101325.0, 0.0},
    {L"Pounds per square inch", L"psi", 6894.757293168, 0.0},
    {L"Torr", L"Torr", 133.3223684210526, 0.0}
};

static const UnitDef UNITS_ANGLE[] = {
    {L"Degrees", L"°", 0.017453292519943295, 0.0},
    {L"Radians", L"rad", 1.0, 0.0},
    {L"Gradians", L"grad", 0.015707963267948967, 0.0},
    {L"Turns", L"turn", 6.283185307179586477, 0.0}
};

static ExtraMode g_mode = MODE_STANDARD;
static ConverterState g_converter;
static ScientificState g_scientific;
static ProgrammerState g_programmer;
static DateState g_date;
static CurrencyRate g_currency[EXTRA_MAX_CURRENCIES];
static int g_currency_count;
static int g_currency_fetching;
static int g_currency_loaded;
static int g_currency_selection_initialized;
static wchar_t g_currency_path[EXTRA_PATH_CAPACITY];
static wchar_t g_currency_date[16];
static wchar_t g_currency_gathered[96];
static wchar_t g_currency_status[160];
static wchar_t g_pending_history_expression[256];
static wchar_t g_pending_history_result[160];
static int g_pending_history;
static ExtraTextSelection g_text_selection;

static int sx(int value, UINT dpi) {
    return MulDiv(value, (int)(dpi ? dpi : 96), 96);
}

static void copy_ascii(char *destination, size_t capacity, const char *source) {
    if (!destination || capacity == 0) return;
    _snprintf(destination, capacity, "%s", source ? source : "");
    destination[capacity - 1] = '\0';
}

static void copy_wide(wchar_t *destination, size_t capacity, const wchar_t *source) {
    if (!destination || capacity == 0) return;
    wcsncpy(destination, source ? source : L"", capacity - 1);
    destination[capacity - 1] = L'\0';
}

static void format_double_ascii(double value, char *output, size_t capacity) {
    if (!isfinite(value)) {
        copy_ascii(output, capacity, "Invalid input");
        return;
    }
    if (fabs(value) < 1e-300) value = 0.0;
    _snprintf(output, capacity, "%.15g", value);
    output[capacity - 1] = '\0';
}

static void format_double_wide(double value, wchar_t *output, size_t capacity) {
    char ascii[96];
    size_t index;
    format_double_ascii(value, ascii, sizeof(ascii));
    for (index = 0; ascii[index] && index + 1 < capacity; ++index) {
        if (ascii[index] == '.') output[index] = L',';
        else if (ascii[index] == '-') output[index] = L'−';
        else output[index] = (wchar_t)(unsigned char)ascii[index];
    }
    output[index] = L'\0';
}

static const UnitDef *unit_table(ExtraMode mode, int *count) {
    const UnitDef *table = NULL;
    int total = 0;
    switch (mode) {
        case MODE_VOLUME: table = UNITS_VOLUME; total = (int)_countof(UNITS_VOLUME); break;
        case MODE_LENGTH: table = UNITS_LENGTH; total = (int)_countof(UNITS_LENGTH); break;
        case MODE_WEIGHT: table = UNITS_WEIGHT; total = (int)_countof(UNITS_WEIGHT); break;
        case MODE_TEMPERATURE: table = UNITS_TEMPERATURE; total = (int)_countof(UNITS_TEMPERATURE); break;
        case MODE_ENERGY: table = UNITS_ENERGY; total = (int)_countof(UNITS_ENERGY); break;
        case MODE_AREA: table = UNITS_AREA; total = (int)_countof(UNITS_AREA); break;
        case MODE_SPEED: table = UNITS_SPEED; total = (int)_countof(UNITS_SPEED); break;
        case MODE_TIME: table = UNITS_TIME; total = (int)_countof(UNITS_TIME); break;
        case MODE_POWER: table = UNITS_POWER; total = (int)_countof(UNITS_POWER); break;
        case MODE_DATA: table = UNITS_DATA; total = (int)_countof(UNITS_DATA); break;
        case MODE_PRESSURE: table = UNITS_PRESSURE; total = (int)_countof(UNITS_PRESSURE); break;
        case MODE_ANGLE: table = UNITS_ANGLE; total = (int)_countof(UNITS_ANGLE); break;
        default: break;
    }
    if (count) *count = total;
    return table;
}

int extras_unit_count(ExtraMode mode) {
    int count = 0;
    if (mode == MODE_CURRENCY) return g_currency_count;
    unit_table(mode, &count);
    return count;
}

const wchar_t *extras_unit_name(ExtraMode mode, int index) {
    int count;
    const UnitDef *table;
    if (mode == MODE_CURRENCY) {
        return index >= 0 && index < g_currency_count ? g_currency[index].label : L"";
    }
    table = unit_table(mode, &count);
    return table && index >= 0 && index < count ? table[index].name : L"";
}

double extras_convert_value(ExtraMode mode, int from_index, int to_index, double value) {
    int count;
    const UnitDef *table;
    double canonical;
    if (mode == MODE_CURRENCY) {
        if (from_index < 0 || from_index >= g_currency_count ||
            to_index < 0 || to_index >= g_currency_count ||
            g_currency[from_index].rate == 0.0) return 0.0;
        return value * g_currency[to_index].rate / g_currency[from_index].rate;
    }
    table = unit_table(mode, &count);
    if (!table || from_index < 0 || from_index >= count ||
        to_index < 0 || to_index >= count || table[to_index].scale == 0.0) return 0.0;
    canonical = value * table[from_index].scale + table[from_index].offset;
    return (canonical - table[to_index].offset) / table[to_index].scale;
}

const wchar_t *extras_mode_name(ExtraMode mode) {
    static const wchar_t *const names[MODE_COUNT] = {
        L"Standard", L"Scientific", L"Programmer", L"Date calculation",
        L"Currency", L"Volume", L"Length", L"Weight and mass", L"Temperature",
        L"Energy", L"Area", L"Speed", L"Time", L"Power", L"Data", L"Pressure", L"Angle"
    };
    return mode >= 0 && mode < MODE_COUNT ? names[mode] : L"Calculator";
}

static void build_currency_path(void) {
    static const wchar_t filename[] = L"Windows Calculator Currency Rates.json";
    wchar_t documents[EXTRA_PATH_CAPACITY];
    wchar_t module_path[EXTRA_PATH_CAPACITY];
    wchar_t *slash;
    size_t length;
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT,
                                   documents))) {
        length = wcslen(documents);
        _snwprintf(g_currency_path, _countof(g_currency_path), L"%ls%ls%ls",
                   documents, length && documents[length - 1] == L'\\' ? L"" : L"\\",
                   filename);
        g_currency_path[_countof(g_currency_path) - 1] = L'\0';
        return;
    }
    module_path[0] = L'\0';
    if (GetModuleFileNameW(NULL, module_path, _countof(module_path)) > 0) {
        slash = wcsrchr(module_path, L'\\');
        if (slash) *slash = L'\0';
        _snwprintf(g_currency_path, _countof(g_currency_path), L"%ls%ls%ls",
                   module_path, slash ? L"\\" : L"", filename);
        g_currency_path[_countof(g_currency_path) - 1] = L'\0';
        return;
    }
    copy_wide(g_currency_path, _countof(g_currency_path), filename);
}

static char *read_file_bytes(const wchar_t *path, DWORD *size_out) {
    HANDLE file;
    LARGE_INTEGER size;
    DWORD read = 0;
    char *buffer;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 4 * 1024 * 1024) {
        CloseHandle(file);
        return NULL;
    }
    buffer = (char *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size.QuadPart + 1);
    if (!buffer) {
        CloseHandle(file);
        return NULL;
    }
    if (!ReadFile(file, buffer, (DWORD)size.QuadPart, &read, NULL) ||
        read != (DWORD)size.QuadPart) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);
    buffer[read] = '\0';
    if (size_out) *size_out = read;
    return buffer;
}

static const char *json_value(const char *object, const char *key) {
    const char *cursor = strstr(object, key);
    if (!cursor) return NULL;
    cursor += strlen(key);
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor != ':') return NULL;
    ++cursor;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    return cursor;
}

static int compare_currency(const void *left, const void *right) {
    const CurrencyRate *a = (const CurrencyRate *)left;
    const CurrencyRate *b = (const CurrencyRate *)right;
    int compared = _wcsicmp(a->location, b->location);
    if (!compared) compared = _wcsicmp(a->name, b->name);
    if (!compared) compared = _wcsicmp(a->code, b->code);
    return compared;
}

typedef struct CurrencyOverride {
    const wchar_t *code;
    const wchar_t *location;
    const wchar_t *name;
    const wchar_t *symbol;
    int symbol_prefix;
} CurrencyOverride;

static const CurrencyOverride CURRENCY_OVERRIDES[] = {
    {L"AED", L"United Arab Emirates", L"Dirham", L"د.إ", 0},
    {L"AUD", L"Australia", L"Dollar", L"A$", 1},
    {L"BRL", L"Brazil", L"Real", L"R$", 1},
    {L"CAD", L"Canada", L"Dollar", L"C$", 1},
    {L"CHF", L"Switzerland", L"Franc", L"CHF", 1},
    {L"CNY", L"China", L"Yuan", L"¥", 1},
    {L"CZK", L"Czech Republic", L"Koruna", L"Kč", 0},
    {L"DKK", L"Denmark", L"Krone", L"kr.", 0},
    {L"EGP", L"Egypt", L"Pound", L"ج.م", 0},
    {L"EUR", L"Europe", L"Euro", L"€", 1},
    {L"GBP", L"United Kingdom", L"Pound", L"£", 1},
    {L"IDR", L"Indonesia", L"Rupiah", L"Rp", 1},
    {L"ILS", L"Israel", L"New Shekel", L"₪", 1},
    {L"INR", L"India", L"Rupee", L"₹", 1},
    {L"JMD", L"Jamaica", L"Dollar", L"J$", 1},
    {L"JPY", L"Japan", L"Yen", L"¥", 1},
    {L"KRW", L"South Korea", L"Won", L"₩", 1},
    {L"LYD", L"Libya", L"Dinar", L"ل.د", 0},
    {L"MXN", L"Mexico", L"Peso", L"MX$", 1},
    {L"NOK", L"Norway", L"Krone", L"kr", 0},
    {L"NZD", L"New Zealand", L"Dollar", L"NZ$", 1},
    {L"PHP", L"Philippines", L"Peso", L"₱", 1},
    {L"PLN", L"Poland", L"Złoty", L"zł", 0},
    {L"RUB", L"Russia", L"Ruble", L"₽", 0},
    {L"SEK", L"Sweden", L"Krona", L"kr", 0},
    {L"THB", L"Thailand", L"Baht", L"฿", 1},
    {L"TRY", L"Türkiye", L"Lira", L"₺", 1},
    {L"USD", L"United States", L"Dollar", L"$", 1},
    {L"VND", L"Vietnam", L"Đồng", L"₫", 0},
    {L"XAF", L"Central Africa", L"CFA Franc", L"CFA", 0},
    {L"XAG", L"International", L"Silver (troy ounce)", L"oz t", 0},
    {L"XAU", L"International", L"Gold (troy ounce)", L"oz t", 0},
    {L"XCD", L"East Caribbean", L"Dollar", L"EC$", 1},
    {L"XOF", L"West Africa", L"CFA Franc", L"CFA", 0},
    {L"XPD", L"International", L"Palladium (troy ounce)", L"oz t", 0},
    {L"XPF", L"French overseas collectivities", L"Central Pacific Franc", L"₣", 0},
    {L"XPT", L"International", L"Platinum (troy ounce)", L"oz t", 0},
    {L"ZAR", L"South Africa", L"Rand", L"R", 1}
};

static BOOL CALLBACK currency_locale_callback(LPWSTR locale_name, DWORD flags,
                                               LPARAM parameter) {
    CurrencyRate *currency = (CurrencyRate *)parameter;
    wchar_t code[8] = L"";
    DWORD prefix = 1;
    (void)flags;
    if (!currency ||
        !GetLocaleInfoEx(locale_name, LOCALE_SINTLSYMBOL, code, (int)_countof(code)) ||
        _wcsicmp(code, currency->code) != 0)
        return TRUE;
    GetLocaleInfoEx(locale_name, LOCALE_SENGLISHCOUNTRYNAME,
                    currency->location, (int)_countof(currency->location));
    GetLocaleInfoEx(locale_name, LOCALE_SENGCURRNAME,
                    currency->name, (int)_countof(currency->name));
    GetLocaleInfoEx(locale_name, LOCALE_SCURRENCY,
                    currency->symbol, (int)_countof(currency->symbol));
    if (GetLocaleInfoEx(locale_name, LOCALE_IPOSSYMPRECEDES | LOCALE_RETURN_NUMBER,
                        (LPWSTR)&prefix, (int)(sizeof(prefix) / sizeof(wchar_t))))
        currency->symbol_prefix = prefix != 0;
    return FALSE;
}

static void populate_currency_metadata(CurrencyRate *currency) {
    size_t index;
    if (!currency) return;
    copy_wide(currency->location, _countof(currency->location), L"International");
    copy_wide(currency->name, _countof(currency->name), L"Currency");
    copy_wide(currency->symbol, _countof(currency->symbol), currency->code);
    currency->symbol_prefix = 0;
    EnumSystemLocalesEx(currency_locale_callback, LOCALE_ALL,
                        (LPARAM)currency, NULL);
    for (index = 0; index < _countof(CURRENCY_OVERRIDES); ++index) {
        const CurrencyOverride *override = &CURRENCY_OVERRIDES[index];
        if (_wcsicmp(override->code, currency->code) == 0) {
            copy_wide(currency->location, _countof(currency->location),
                      override->location);
            copy_wide(currency->name, _countof(currency->name), override->name);
            copy_wide(currency->symbol, _countof(currency->symbol), override->symbol);
            currency->symbol_prefix = override->symbol_prefix;
            break;
        }
    }
    _snwprintf(currency->label, _countof(currency->label), L"%ls - %ls",
               currency->location, currency->name);
    currency->label[_countof(currency->label) - 1] = L'\0';
}

static int parse_currency_json(const char *json) {
    const char *object = json;
    int count = 0;
    wchar_t latest_date[16] = L"";
    while (object && *object && count < EXTRA_MAX_CURRENCIES) {
        const char *end;
        const char *quote;
        const char *rate;
        const char *date;
        char code[8];
        char date_text[16];
        char *number_end;
        size_t code_length = 0;
        size_t date_length = 0;
        double value;
        object = strchr(object, '{');
        if (!object) break;
        end = strchr(object, '}');
        if (!end) break;
        quote = json_value(object, "\"quote\"");
        rate = json_value(object, "\"rate\"");
        date = json_value(object, "\"date\"");
        if (!quote || !rate || quote >= end || rate >= end || *quote != '"') {
            object = end + 1;
            continue;
        }
        ++quote;
        while (quote[code_length] && quote + code_length < end &&
               quote[code_length] != '"' && code_length + 1 < sizeof(code)) ++code_length;
        if (quote[code_length] != '"' || code_length != 3) {
            object = end + 1;
            continue;
        }
        memcpy(code, quote, code_length);
        code[code_length] = '\0';
        value = strtod(rate, &number_end);
        if (number_end == rate || !isfinite(value) || value <= 0.0) {
            object = end + 1;
            continue;
        }
        MultiByteToWideChar(CP_UTF8, 0, code, -1, g_currency[count].code,
                            (int)_countof(g_currency[count].code));
        populate_currency_metadata(&g_currency[count]);
        g_currency[count].rate = value;
        ++count;
        if (date && date < end && *date == '"') {
            ++date;
            while (date[date_length] && date + date_length < end &&
                   date[date_length] != '"' && date_length + 1 < sizeof(date_text)) ++date_length;
            if (date_length == 10) {
                memcpy(date_text, date, date_length);
                date_text[date_length] = '\0';
                {
                    wchar_t wide_date[16];
                    MultiByteToWideChar(CP_UTF8, 0, date_text, -1, wide_date,
                                        (int)_countof(wide_date));
                    if (!latest_date[0] || wcscmp(wide_date, latest_date) > 0)
                        copy_wide(latest_date, _countof(latest_date), wide_date);
                }
            }
        }
        object = end + 1;
    }
    if (count <= 1) {
        g_currency_count = 0;
        return 0;
    }
    qsort(g_currency, (size_t)count, sizeof(g_currency[0]), compare_currency);
    g_currency_count = count;
    copy_wide(g_currency_date, _countof(g_currency_date), latest_date);
    return 1;
}

static int load_currency_cache(void) {
    char *json;
    DWORD size;
    HANDLE file;
    FILETIME write_time;
    SYSTEMTIME utc;
    SYSTEMTIME local;
    wchar_t date_buffer[64];
    wchar_t time_buffer[32];
    json = read_file_bytes(g_currency_path, &size);
    (void)size;
    if (!json) return 0;
    g_currency_loaded = parse_currency_json(json);
    HeapFree(GetProcessHeap(), 0, json);
    if (!g_currency_loaded) return 0;
    file = CreateFileW(g_currency_path, FILE_READ_ATTRIBUTES,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        if (GetFileTime(file, NULL, NULL, &write_time) &&
            FileTimeToSystemTime(&write_time, &utc) &&
            SystemTimeToTzSpecificLocalTime(NULL, &utc, &local)) {
            GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &local, NULL,
                           date_buffer, (int)_countof(date_buffer));
            GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &local, NULL,
                           time_buffer, (int)_countof(time_buffer));
            _snwprintf(g_currency_gathered, _countof(g_currency_gathered),
                       L"Rates last gathered: %ls at %ls", date_buffer, time_buffer);
            g_currency_gathered[_countof(g_currency_gathered) - 1] = L'\0';
        }
        CloseHandle(file);
    }
    return 1;
}

static int get_central_europe_time(SYSTEMTIME *local) {
    DYNAMIC_TIME_ZONE_INFORMATION zone;
    DWORD index = 0;
    SYSTEMTIME utc;
    while (EnumDynamicTimeZoneInformation(index, &zone) == ERROR_SUCCESS) {
        if (_wcsicmp(zone.TimeZoneKeyName, L"W. Europe Standard Time") == 0 ||
            _wcsicmp(zone.TimeZoneKeyName, L"Central Europe Standard Time") == 0) {
            GetSystemTime(&utc);
            return SystemTimeToTzSpecificLocalTimeEx(&zone, &utc, local) != 0;
        }
        ++index;
    }
    GetLocalTime(local);
    return 0;
}

static int currency_date_is_friday(void) {
    SYSTEMTIME date;
    FILETIME value;
    SYSTEMTIME normalized;
    int year, month, day;
    if (swscanf(g_currency_date, L"%d-%d-%d", &year, &month, &day) != 3) return 0;
    ZeroMemory(&date, sizeof(date));
    date.wYear = (WORD)year;
    date.wMonth = (WORD)month;
    date.wDay = (WORD)day;
    if (!SystemTimeToFileTime(&date, &value) || !FileTimeToSystemTime(&value, &normalized))
        return 0;
    return normalized.wDayOfWeek == 5;
}

static int currency_cache_younger_than_three_hours(void) {
    HANDLE file;
    FILETIME write_time;
    FILETIME now;
    ULARGE_INTEGER written;
    ULARGE_INTEGER current;
    file = CreateFileW(g_currency_path, FILE_READ_ATTRIBUTES,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileTime(file, NULL, NULL, &write_time)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    GetSystemTimeAsFileTime(&now);
    written.LowPart = write_time.dwLowDateTime;
    written.HighPart = write_time.dwHighDateTime;
    current.LowPart = now.dwLowDateTime;
    current.HighPart = now.dwHighDateTime;
    if (current.QuadPart < written.QuadPart) return 1;
    return current.QuadPart - written.QuadPart < 3ULL * 60ULL * 60ULL * 10000000ULL;
}

static int should_fetch_currency(void) {
    SYSTEMTIME cet;
    get_central_europe_time(&cet);
    if (!g_currency_loaded) return 1;
    if ((cet.wDayOfWeek == 0 || cet.wDayOfWeek == 6) && currency_date_is_friday())
        return 0;
    if (cet.wHour >= 15 && cet.wHour < 18) return 1;
    return !currency_cache_younger_than_three_hours();
}

static void currency_pending_path(wchar_t *path, size_t capacity) {
    _snwprintf(path, capacity, L"%ls.pending", g_currency_path);
    path[capacity - 1] = L'\0';
}

static int save_currency_response(const char *buffer, DWORD size) {
    wchar_t temporary[EXTRA_PATH_CAPACITY];
    HANDLE file;
    DWORD written = 0;
    currency_pending_path(temporary, _countof(temporary));
    file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!WriteFile(file, buffer, size, &written, NULL) || written != size ||
        !FlushFileBuffers(file)) {
        CloseHandle(file);
        DeleteFileW(temporary);
        return 0;
    }
    CloseHandle(file);
    return 1;
}

static DWORD WINAPI currency_thread(void *parameter) {
    HINTERNET session = NULL;
    HINTERNET connection = NULL;
    HINTERNET request = NULL;
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    char *buffer = NULL;
    DWORD capacity = 0;
    DWORD used = 0;
    DWORD available;
    int success = 0;
    HWND owner = (HWND)parameter;
    session = WinHttpOpen(L"PersistentCalculator-Currency/2.0",
                          WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto cleanup;
    WinHttpSetTimeouts(session, 7000, 7000, 10000, 10000);
    connection = WinHttpConnect(session, L"api.frankfurter.dev",
                                INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) goto cleanup;
    request = WinHttpOpenRequest(connection, L"GET", L"/v2/rates?base=EUR",
                                 NULL, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) goto cleanup;
    {
        DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY,
                              &redirect_policy, sizeof(redirect_policy))) goto cleanup;
    }
    if (!WinHttpSendRequest(request, L"Accept: application/json\r\n",
                            (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, NULL)) goto cleanup;
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                             WINHTTP_NO_HEADER_INDEX) || status != 200) goto cleanup;
    do {
        DWORD read = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) goto cleanup;
        if (!available) break;
        if (used + available + 1 > 4 * 1024 * 1024) goto cleanup;
        if (used + available + 1 > capacity) {
            DWORD new_capacity = capacity ? capacity * 2 : 65536;
            char *resized;
            while (new_capacity < used + available + 1) new_capacity *= 2;
            resized = buffer
                ? (char *)HeapReAlloc(GetProcessHeap(), 0, buffer, new_capacity)
                : (char *)HeapAlloc(GetProcessHeap(), 0, new_capacity);
            if (!resized) goto cleanup;
            buffer = resized;
            capacity = new_capacity;
        }
        if (!WinHttpReadData(request, buffer + used, available, &read) || read == 0)
            goto cleanup;
        used += read;
    } while (available);
    if (!buffer || used < 16) goto cleanup;
    buffer[used] = '\0';
    success = save_currency_response(buffer, used);
cleanup:
    if (buffer) HeapFree(GetProcessHeap(), 0, buffer);
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    PostMessageW(owner, EXTRA_CURRENCY_READY, 0, success ? 1 : 0);
    return 0;
}

static void request_currency(HWND owner) {
    HANDLE thread;
    if (g_currency_fetching || !should_fetch_currency()) {
        if (g_currency_loaded)
            copy_wide(g_currency_status, _countof(g_currency_status),
                      L"Daily reference rates from Frankfurter");
        return;
    }
    g_currency_fetching = 1;
    copy_wide(g_currency_status, _countof(g_currency_status), L"Refreshing currency rates…");
    thread = CreateThread(NULL, 0, currency_thread, owner, 0, NULL);
    if (thread) CloseHandle(thread);
    else {
        g_currency_fetching = 0;
        copy_wide(g_currency_status, _countof(g_currency_status),
                  L"Could not start the currency update");
    }
}

static void ensure_currency_selection(void) {
    int index;
    int eur = 0;
    int usd = g_currency_count > 1 ? 1 : 0;
    if (g_currency_selection_initialized || g_currency_count <= 0) return;
    for (index = 0; index < g_currency_count; ++index) {
        if (_wcsicmp(g_currency[index].code, L"EUR") == 0) eur = index;
        if (_wcsicmp(g_currency[index].code, L"USD") == 0) usd = index;
    }
    g_converter.from_index[MODE_CURRENCY] = eur;
    g_converter.to_index[MODE_CURRENCY] = usd;
    g_currency_selection_initialized = 1;
}

static void set_pending_history(const wchar_t *expression, const wchar_t *result) {
    copy_wide(g_pending_history_expression, _countof(g_pending_history_expression), expression);
    copy_wide(g_pending_history_result, _countof(g_pending_history_result), result);
    g_pending_history = expression && *expression && result && *result;
}

int extras_take_history(wchar_t *expression, size_t expression_capacity,
                        wchar_t *result, size_t result_capacity) {
    if (!g_pending_history) return 0;
    copy_wide(expression, expression_capacity, g_pending_history_expression);
    copy_wide(result, result_capacity, g_pending_history_result);
    g_pending_history = 0;
    return 1;
}

static void scientific_reset(void) {
    ZeroMemory(&g_scientific, sizeof(g_scientific));
    copy_ascii(g_scientific.display, sizeof(g_scientific.display), "0");
    g_scientific.new_input = 1;
}

static double scientific_value(void) {
    return g_scientific.error ? 0.0 : strtod(g_scientific.display, NULL);
}

static void scientific_set(double value) {
    if (!isfinite(value)) {
        copy_ascii(g_scientific.display, sizeof(g_scientific.display), "Invalid input");
        g_scientific.error = 1;
        g_scientific.new_input = 1;
        return;
    }
    if (g_scientific.f_e)
        _snprintf(g_scientific.display, sizeof(g_scientific.display), "%.12e", value);
    else
        format_double_ascii(value, g_scientific.display, sizeof(g_scientific.display));
    g_scientific.display[sizeof(g_scientific.display) - 1] = '\0';
    g_scientific.error = 0;
    g_scientific.new_input = 1;
}

static double angle_to_radians(double value) {
    if (g_scientific.angle_mode == 0) return value * 3.14159265358979323846 / 180.0;
    if (g_scientific.angle_mode == 2) return value * 3.14159265358979323846 / 200.0;
    return value;
}

static double radians_to_angle(double value) {
    if (g_scientific.angle_mode == 0) return value * 180.0 / 3.14159265358979323846;
    if (g_scientific.angle_mode == 2) return value * 200.0 / 3.14159265358979323846;
    return value;
}

static int scientific_apply_binary(double left, double right, char operation, double *answer) {
    switch (operation) {
        case '+': *answer = left + right; break;
        case '-': *answer = left - right; break;
        case '*': *answer = left * right; break;
        case '/':
            if (right == 0.0) return 0;
            *answer = left / right;
            break;
        case '^': *answer = pow(left, right); break;
        case '%':
            if (right == 0.0) return 0;
            *answer = fmod(left, right);
            break;
        default: return 0;
    }
    return isfinite(*answer);
}

static void scientific_digit(int digit) {
    size_t length;
    if (digit < 0 || digit > 9) return;
    if (g_scientific.error || g_scientific.new_input) {
        copy_ascii(g_scientific.display, sizeof(g_scientific.display), "0");
        g_scientific.new_input = 0;
        g_scientific.error = 0;
    }
    length = strlen(g_scientific.display);
    if (length >= 18) return;
    if (strcmp(g_scientific.display, "0") == 0) {
        g_scientific.display[0] = (char)('0' + digit);
        return;
    }
    g_scientific.display[length] = (char)('0' + digit);
    g_scientific.display[length + 1] = '\0';
}

static void scientific_decimal(void) {
    if (g_scientific.error || g_scientific.new_input) {
        copy_ascii(g_scientific.display, sizeof(g_scientific.display), "0.");
        g_scientific.new_input = 0;
        g_scientific.error = 0;
    } else if (!strchr(g_scientific.display, '.') && !strchr(g_scientific.display, 'e') &&
               strlen(g_scientific.display) + 1 < sizeof(g_scientific.display)) {
        strcat(g_scientific.display, ".");
    }
}

static void scientific_backspace(void) {
    size_t length;
    if (g_scientific.error || g_scientific.new_input) return;
    length = strlen(g_scientific.display);
    if (length) g_scientific.display[length - 1] = '\0';
    if (!g_scientific.display[0] || strcmp(g_scientific.display, "-") == 0) {
        copy_ascii(g_scientific.display, sizeof(g_scientific.display), "0");
        g_scientific.new_input = 1;
    }
}

static void scientific_operator(char operation) {
    double current = scientific_value();
    double answer;
    if (g_scientific.error) return;
    if (g_scientific.has_accumulator && !g_scientific.new_input) {
        if (!scientific_apply_binary(g_scientific.accumulator, current,
                                     g_scientific.pending, &answer)) {
            scientific_set(NAN);
            return;
        }
        g_scientific.accumulator = answer;
        scientific_set(answer);
    } else if (!g_scientific.has_accumulator) {
        g_scientific.accumulator = current;
        g_scientific.has_accumulator = 1;
    }
    g_scientific.pending = operation;
    g_scientific.new_input = 1;
}

static void scientific_equals(void) {
    double left;
    double right;
    double answer;
    wchar_t expression[256];
    wchar_t result[160];
    wchar_t left_text[96];
    wchar_t right_text[96];
    wchar_t op[4] = L"";
    if (!g_scientific.has_accumulator || !g_scientific.pending || g_scientific.error) return;
    left = g_scientific.accumulator;
    right = g_scientific.new_input ? left : scientific_value();
    if (!scientific_apply_binary(left, right, g_scientific.pending, &answer)) {
        scientific_set(NAN);
        return;
    }
    format_double_wide(left, left_text, _countof(left_text));
    format_double_wide(right, right_text, _countof(right_text));
    format_double_wide(answer, result, _countof(result));
    op[0] = g_scientific.pending == '*' ? L'×' :
            g_scientific.pending == '/' ? L'÷' :
            g_scientific.pending == '-' ? L'−' :
            g_scientific.pending == '^' ? L'^' :
            g_scientific.pending == '%' ? L'm' : (wchar_t)g_scientific.pending;
    if (g_scientific.pending == '%')
        _snwprintf(expression, _countof(expression), L"%ls mod %ls =", left_text, right_text);
    else
        _snwprintf(expression, _countof(expression), L"%ls %ls %ls =", left_text, op, right_text);
    expression[_countof(expression) - 1] = L'\0';
    set_pending_history(expression, result);
    scientific_set(answer);
    g_scientific.has_accumulator = 0;
    g_scientific.pending = 0;
    g_scientific.operand = right;
}

static void scientific_open_group(void) {
    if (g_scientific.group_active || g_scientific.error) return;
    if (g_scientific.has_accumulator) {
        g_scientific.outer_accumulator = g_scientific.accumulator;
        g_scientific.outer_pending = g_scientific.pending;
    } else {
        g_scientific.outer_accumulator = 0.0;
        g_scientific.outer_pending = 0;
    }
    g_scientific.group_active = 1;
    g_scientific.has_accumulator = 0;
    g_scientific.pending = 0;
    g_scientific.accumulator = 0.0;
    copy_ascii(g_scientific.display, sizeof(g_scientific.display), "0");
    g_scientific.new_input = 1;
}

static void scientific_close_group(void) {
    double inner = scientific_value();
    double answer;
    if (!g_scientific.group_active || g_scientific.error) return;
    if (g_scientific.has_accumulator && g_scientific.pending &&
        !g_scientific.new_input) {
        if (!scientific_apply_binary(g_scientific.accumulator, inner,
                                     g_scientific.pending, &inner)) {
            scientific_set(NAN);
            return;
        }
    }
    if (g_scientific.outer_pending) {
        if (!scientific_apply_binary(g_scientific.outer_accumulator, inner,
                                     g_scientific.outer_pending, &answer)) {
            scientific_set(NAN);
            return;
        }
    } else {
        answer = inner;
    }
    g_scientific.group_active = 0;
    g_scientific.outer_pending = 0;
    g_scientific.has_accumulator = 0;
    g_scientific.pending = 0;
    scientific_set(answer);
}

static void scientific_unary(int operation) {
    double input = scientific_value();
    double answer = 0.0;
    double integer_part;
    wchar_t expression[256];
    wchar_t result[160];
    wchar_t input_text[96];
    const wchar_t *name = L"";
    int ok = 1;
    if (g_scientific.error) return;
    switch (operation) {
        case 0:
            if (g_scientific.hyperbolic && g_scientific.inverse) {
                answer = asinh(input);
                name = L"asinh";
            } else if (g_scientific.hyperbolic) {
                answer = sinh(input);
                name = L"sinh";
            } else if (g_scientific.inverse) {
                if (input < -1.0 || input > 1.0) ok = 0;
                else answer = radians_to_angle(asin(input));
                name = L"asin";
            } else {
                answer = sin(angle_to_radians(input));
                name = L"sin";
            }
            break;
        case 1:
            if (g_scientific.hyperbolic && g_scientific.inverse) {
                if (input < 1.0) ok = 0;
                else answer = acosh(input);
                name = L"acosh";
            } else if (g_scientific.hyperbolic) {
                answer = cosh(input);
                name = L"cosh";
            } else if (g_scientific.inverse) {
                if (input < -1.0 || input > 1.0) ok = 0;
                else answer = radians_to_angle(acos(input));
                name = L"acos";
            } else {
                answer = cos(angle_to_radians(input));
                name = L"cos";
            }
            break;
        case 2:
            if (g_scientific.hyperbolic && g_scientific.inverse) {
                if (input <= -1.0 || input >= 1.0) ok = 0;
                else answer = atanh(input);
                name = L"atanh";
            } else if (g_scientific.hyperbolic) {
                answer = tanh(input);
                name = L"tanh";
            } else if (g_scientific.inverse) {
                answer = radians_to_angle(atan(input));
                name = L"atan";
            } else {
                answer = tan(angle_to_radians(input));
                name = L"tan";
            }
            break;
        case 3:
            if (input <= 0.0) ok = 0;
            else answer = log10(input);
            name = L"log";
            break;
        case 4:
            if (input <= 0.0) ok = 0;
            else answer = log(input);
            name = L"ln";
            break;
        case 5:
            if (input < 0.0) ok = 0;
            else answer = sqrt(input);
            name = L"sqrt";
            break;
        case 6: answer = input * input; name = L"sqr"; break;
        case 7: scientific_operator('^'); return;
        case 8: answer = pow(10.0, input); name = L"10^"; break;
        case 9: answer = exp(input); name = L"exp"; break;
        case 10: answer = fabs(input); name = L"abs"; break;
        case 11:
            if (input == 0.0) ok = 0;
            else answer = 1.0 / input;
            name = L"1/";
            break;
        case 12:
            if (input < 0.0 || input > 170.0 || modf(input, &integer_part) != 0.0) ok = 0;
            else {
                int index;
                answer = 1.0;
                for (index = 2; index <= (int)input; ++index) answer *= index;
            }
            name = L"fact";
            break;
        case 13:
            answer = g_scientific.hyperbolic
                         ? 1.0 / cosh(input)
                         : 1.0 / cos(angle_to_radians(input));
            name = g_scientific.hyperbolic ? L"sech" : L"sec";
            break;
        case 14: {
            double divisor = g_scientific.hyperbolic
                                 ? sinh(input) : sin(angle_to_radians(input));
            if (divisor == 0.0) ok = 0;
            else answer = 1.0 / divisor;
            name = g_scientific.hyperbolic ? L"csch" : L"csc";
            break;
        }
        case 15: {
            double divisor = g_scientific.hyperbolic
                                 ? tanh(input) : tan(angle_to_radians(input));
            if (divisor == 0.0) ok = 0;
            else answer = 1.0 / divisor;
            name = g_scientific.hyperbolic ? L"coth" : L"cot";
            break;
        }
        case 16: answer = floor(input); name = L"floor"; break;
        case 17: answer = ceil(input); name = L"ceil"; break;
        case 18: {
            double absolute = fabs(input);
            double degrees = floor(absolute);
            double minutes_value = (absolute - degrees) * 60.0;
            double minutes = floor(minutes_value);
            double seconds = (minutes_value - minutes) * 60.0;
            answer = degrees + minutes / 100.0 + seconds / 10000.0;
            if (input < 0.0) answer = -answer;
            name = L"dms";
            break;
        }
        case 19: {
            double absolute = fabs(input);
            double degrees = floor(absolute);
            double minutes_value = (absolute - degrees) * 100.0;
            double minutes = floor(minutes_value);
            double seconds = (minutes_value - minutes) * 100.0;
            answer = degrees + minutes / 60.0 + seconds / 3600.0;
            if (input < 0.0) answer = -answer;
            name = L"deg";
            break;
        }
        case 20:
            answer = (double)(GetTickCount64() % 1000000ULL) / 1000000.0;
            name = L"rand";
            break;
        default: return;
    }
    if (!ok || !isfinite(answer)) {
        scientific_set(NAN);
        return;
    }
    format_double_wide(input, input_text, _countof(input_text));
    format_double_wide(answer, result, _countof(result));
    _snwprintf(expression, _countof(expression), L"%ls(%ls) =", name, input_text);
    expression[_countof(expression) - 1] = L'\0';
    set_pending_history(expression, result);
    scientific_set(answer);
}

static uint64_t programmer_mask(void) {
    return g_programmer.bits >= 64 ? UINT64_MAX : ((1ULL << g_programmer.bits) - 1ULL);
}

static void programmer_reset(void) {
    ZeroMemory(&g_programmer, sizeof(g_programmer));
    g_programmer.base = 10;
    g_programmer.bits = 64;
    g_programmer.new_input = 1;
}

static int programmer_apply(uint64_t left, uint64_t right, int operation, uint64_t *answer) {
    uint64_t mask = programmer_mask();
    unsigned count = (unsigned)(right % (uint64_t)g_programmer.bits);
    switch (operation) {
        case '+': *answer = left + right; break;
        case '-': *answer = left - right; break;
        case '*': *answer = left * right; break;
        case '/': if (!right) return 0; *answer = left / right; break;
        case '%': if (!right) return 0; *answer = left % right; break;
        case '&': *answer = left & right; break;
        case '|': *answer = left | right; break;
        case '^': *answer = left ^ right; break;
        case 'A': *answer = ~(left & right); break;
        case 'O': *answer = ~(left | right); break;
        case '<':
            if (g_programmer.shift_mode == 2) {
                *answer = count ? ((left << count) |
                          (left >> (g_programmer.bits - count))) : left;
            } else if (g_programmer.shift_mode == 3) {
                unsigned step;
                *answer = left;
                for (step = 0; step < count; ++step) {
                    int outgoing = (*answer >> (g_programmer.bits - 1)) & 1ULL;
                    *answer = ((*answer << 1) | (uint64_t)g_programmer.carry) & mask;
                    g_programmer.carry = outgoing;
                }
            } else {
                *answer = left << count;
            }
            break;
        case '>':
            if (g_programmer.shift_mode == 0) {
                uint64_t sign = 1ULL << (g_programmer.bits - 1);
                int64_t signed_left = (left & sign)
                                          ? (int64_t)(left | ~mask)
                                          : (int64_t)left;
                *answer = (uint64_t)(signed_left >> count);
            } else if (g_programmer.shift_mode == 2) {
                *answer = count ? ((left >> count) |
                          (left << (g_programmer.bits - count))) : left;
            } else if (g_programmer.shift_mode == 3) {
                unsigned step;
                *answer = left;
                for (step = 0; step < count; ++step) {
                    int outgoing = (int)(*answer & 1ULL);
                    *answer = (*answer >> 1) |
                              ((uint64_t)g_programmer.carry <<
                               (g_programmer.bits - 1));
                    g_programmer.carry = outgoing;
                }
            } else {
                *answer = left >> count;
            }
            break;
        default: return 0;
    }
    *answer &= mask;
    return 1;
}

static void programmer_digit(int digit) {
    uint64_t mask = programmer_mask();
    if (digit < 0 || digit >= g_programmer.base) return;
    if (g_programmer.new_input || g_programmer.error) {
        g_programmer.value = 0;
        g_programmer.new_input = 0;
        g_programmer.error = 0;
    }
    if (g_programmer.value <= (mask - (uint64_t)digit) / (uint64_t)g_programmer.base)
        g_programmer.value = (g_programmer.value * (uint64_t)g_programmer.base +
                              (uint64_t)digit) & mask;
}

static void programmer_operator(int operation) {
    uint64_t answer;
    if (g_programmer.error) return;
    if (g_programmer.has_accumulator && !g_programmer.new_input) {
        if (!programmer_apply(g_programmer.accumulator, g_programmer.value,
                              g_programmer.pending, &answer)) {
            g_programmer.error = 1;
            return;
        }
        g_programmer.accumulator = answer;
        g_programmer.value = answer;
    } else if (!g_programmer.has_accumulator) {
        g_programmer.accumulator = g_programmer.value;
        g_programmer.has_accumulator = 1;
    }
    g_programmer.pending = operation;
    g_programmer.new_input = 1;
}

static void programmer_equals(void) {
    uint64_t right;
    uint64_t answer;
    wchar_t expression[256];
    wchar_t result[160];
    wchar_t op[12];
    if (!g_programmer.has_accumulator || !g_programmer.pending || g_programmer.error) return;
    right = g_programmer.new_input ? g_programmer.accumulator : g_programmer.value;
    if (!programmer_apply(g_programmer.accumulator, right, g_programmer.pending, &answer)) {
        g_programmer.error = 1;
        return;
    }
    if (g_programmer.pending == '&') copy_wide(op, _countof(op), L"AND");
    else if (g_programmer.pending == '|') copy_wide(op, _countof(op), L"OR");
    else if (g_programmer.pending == '^') copy_wide(op, _countof(op), L"XOR");
    else if (g_programmer.pending == 'A') copy_wide(op, _countof(op), L"NAND");
    else if (g_programmer.pending == 'O') copy_wide(op, _countof(op), L"NOR");
    else if (g_programmer.pending == '<') copy_wide(op, _countof(op), L"<<");
    else if (g_programmer.pending == '>') copy_wide(op, _countof(op), L">>");
    else {
        op[0] = g_programmer.pending == '*' ? L'×' :
                g_programmer.pending == '/' ? L'÷' :
                g_programmer.pending == '-' ? L'−' : (wchar_t)g_programmer.pending;
        op[1] = L'\0';
    }
    _snwprintf(expression, _countof(expression), L"%llu %ls %llu =",
               (unsigned long long)g_programmer.accumulator, op,
               (unsigned long long)right);
    _snwprintf(result, _countof(result), L"%llu", (unsigned long long)answer);
    expression[_countof(expression) - 1] = L'\0';
    result[_countof(result) - 1] = L'\0';
    set_pending_history(expression, result);
    g_programmer.value = answer;
    g_programmer.has_accumulator = 0;
    g_programmer.pending = 0;
    g_programmer.new_input = 1;
}

static void programmer_open_group(void) {
    if (g_programmer.group_active || g_programmer.error) return;
    if (g_programmer.has_accumulator) {
        g_programmer.outer_accumulator = g_programmer.accumulator;
        g_programmer.outer_pending = g_programmer.pending;
    } else {
        g_programmer.outer_accumulator = 0;
        g_programmer.outer_pending = 0;
    }
    g_programmer.group_active = 1;
    g_programmer.has_accumulator = 0;
    g_programmer.pending = 0;
    g_programmer.accumulator = 0;
    g_programmer.value = 0;
    g_programmer.new_input = 1;
}

static void programmer_close_group(void) {
    uint64_t inner = g_programmer.value;
    uint64_t answer;
    if (!g_programmer.group_active || g_programmer.error) return;
    if (g_programmer.has_accumulator && g_programmer.pending &&
        !g_programmer.new_input) {
        if (!programmer_apply(g_programmer.accumulator, inner,
                              g_programmer.pending, &inner)) {
            g_programmer.error = 1;
            return;
        }
    }
    if (g_programmer.outer_pending) {
        if (!programmer_apply(g_programmer.outer_accumulator, inner,
                              g_programmer.outer_pending, &answer)) {
            g_programmer.error = 1;
            return;
        }
    } else {
        answer = inner;
    }
    g_programmer.group_active = 0;
    g_programmer.outer_pending = 0;
    g_programmer.has_accumulator = 0;
    g_programmer.pending = 0;
    g_programmer.value = answer & programmer_mask();
    g_programmer.new_input = 1;
}

static int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int result = days[month - 1];
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) ++result;
    return result;
}

static void clamp_date(SYSTEMTIME *date) {
    int maximum;
    if (date->wYear < 1601) date->wYear = 1601;
    if (date->wYear > 9999) date->wYear = 9999;
    if (date->wMonth < 1) date->wMonth = 1;
    if (date->wMonth > 12) date->wMonth = 12;
    maximum = days_in_month(date->wYear, date->wMonth);
    if (date->wDay < 1) date->wDay = 1;
    if (date->wDay > maximum) date->wDay = (WORD)maximum;
}

static void add_days(SYSTEMTIME *date, int days) {
    FILETIME file_time;
    ULARGE_INTEGER value;
    if (!SystemTimeToFileTime(date, &file_time)) return;
    value.LowPart = file_time.dwLowDateTime;
    value.HighPart = file_time.dwHighDateTime;
    if (days < 0 && value.QuadPart < (uint64_t)(-days) * 864000000000ULL) return;
    value.QuadPart += (int64_t)days * 864000000000LL;
    file_time.dwLowDateTime = value.LowPart;
    file_time.dwHighDateTime = value.HighPart;
    FileTimeToSystemTime(&file_time, date);
}

static void adjust_date(SYSTEMTIME *date, int unit, int amount) {
    int value;
    if (unit == 0) add_days(date, amount);
    else if (unit == 1) {
        value = (int)date->wYear * 12 + (int)date->wMonth - 1 + amount;
        if (value < 1601 * 12) value = 1601 * 12;
        if (value > 9999 * 12 + 11) value = 9999 * 12 + 11;
        date->wYear = (WORD)(value / 12);
        date->wMonth = (WORD)(value % 12 + 1);
        clamp_date(date);
    } else {
        value = (int)date->wYear + amount;
        if (value < 1601) value = 1601;
        if (value > 9999) value = 9999;
        date->wYear = (WORD)value;
        clamp_date(date);
    }
}

static long long date_difference_days(const SYSTEMTIME *first, const SYSTEMTIME *second) {
    FILETIME a;
    FILETIME b;
    ULARGE_INTEGER left;
    ULARGE_INTEGER right;
    if (!SystemTimeToFileTime(first, &a) || !SystemTimeToFileTime(second, &b)) return 0;
    left.LowPart = a.dwLowDateTime;
    left.HighPart = a.dwHighDateTime;
    right.LowPart = b.dwLowDateTime;
    right.HighPart = b.dwHighDateTime;
    if (right.QuadPart >= left.QuadPart)
        return (long long)((right.QuadPart - left.QuadPart) / 864000000000ULL);
    return (long long)((left.QuadPart - right.QuadPart) / 864000000000ULL);
}

void extras_initialize(HWND owner) {
    int index;
    SYSTEMTIME today;
    (void)owner;
    ZeroMemory(&g_converter, sizeof(g_converter));
    for (index = 0; index < MODE_COUNT; ++index) {
        copy_ascii(g_converter.input[index], sizeof(g_converter.input[index]), "0");
        g_converter.new_input[index] = 1;
        g_converter.from_index[index] = 0;
        g_converter.to_index[index] = 1;
    }
    scientific_reset();
    programmer_reset();
    GetLocalTime(&today);
    g_date.first = today;
    g_date.second = today;
    add_days(&g_date.second, 1);
    g_date.add_amount = 1;
    build_currency_path();
    load_currency_cache();
    copy_wide(g_currency_status, _countof(g_currency_status),
              g_currency_loaded ? L"Daily reference rates from Frankfurter"
                                : L"Open Currency to gather daily rates");
}

void extras_shutdown(void) {
}

void extras_set_mode(HWND owner, ExtraMode mode) {
    int count;
    g_mode = mode >= 0 && mode < MODE_COUNT ? mode : MODE_STANDARD;
    extras_clear_text_selection();
    g_converter.picker_open = 0;
    g_converter.picker_search[0] = L'\0';
    g_converter.picker_search_tick = 0;
    g_scientific.popup = 0;
    g_programmer.popup = 0;
    g_date.calendar_target = 0;
    if (g_mode == MODE_CURRENCY) {
        if (!g_currency_loaded) load_currency_cache();
        count = g_currency_count;
        if (count > 0) {
            ensure_currency_selection();
            if (g_converter.from_index[MODE_CURRENCY] >= count ||
                g_converter.to_index[MODE_CURRENCY] >= count) {
                g_currency_selection_initialized = 0;
                ensure_currency_selection();
            }
        }
        request_currency(owner);
    }
}

ExtraMode extras_mode(void) {
    return g_mode;
}

void extras_currency_ready(HWND owner, LPARAM status) {
    wchar_t pending[EXTRA_PATH_CAPACITY];
    char *json = NULL;
    DWORD size = 0;
    int accepted = 0;
    g_currency_fetching = 0;
    currency_pending_path(pending, _countof(pending));
    if (status) {
        json = read_file_bytes(pending, &size);
        if (json && parse_currency_json(json) &&
            MoveFileExW(pending, g_currency_path,
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) &&
            load_currency_cache()) {
            accepted = 1;
        }
    }
    if (json) HeapFree(GetProcessHeap(), 0, json);
    DeleteFileW(pending);
    if (accepted) {
        ensure_currency_selection();
        copy_wide(g_currency_status, _countof(g_currency_status),
                  L"Daily reference rates from Frankfurter");
    } else if (g_currency_loaded) {
        load_currency_cache();
        copy_wide(g_currency_status, _countof(g_currency_status),
                  L"Couldn't refresh; using the saved rates");
    } else {
        copy_wide(g_currency_status, _countof(g_currency_status),
                  L"Couldn't gather rates. Check your connection.");
    }
    InvalidateRect(owner, NULL, FALSE);
}

static HFONT extra_font(UINT dpi, int points, int weight) {
    LOGFONTW font;
    ZeroMemory(&font, sizeof(font));
    font.lfHeight = -MulDiv(points, (int)(dpi ? dpi : 96), 72);
    font.lfWeight = weight;
    font.lfQuality = CLEARTYPE_QUALITY;
    copy_wide(font.lfFaceName, _countof(font.lfFaceName), L"Segoe UI");
    return CreateFontIndirectW(&font);
}

static void fill_color(HDC dc, const RECT *rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void round_color(HDC dc, const RECT *rect, COLORREF color, int radius) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect->left, rect->top, rect->right, rect->bottom, radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void text_color(HDC dc, const wchar_t *text, RECT rect, HFONT font,
                       COLORREF color, UINT format) {
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text ? text : L"", -1, &rect, format | DT_NOPREFIX);
    SelectObject(dc, old);
}

static void fit_text(HDC dc, const wchar_t *text, RECT rect, UINT dpi,
                     int points, int minimum, int weight, COLORREF color,
                     UINT format) {
    HFONT font = NULL;
    HGDIOBJ old;
    SIZE size;
    int selected = points;
    int available = rect.right - rect.left;
    do {
        if (font) DeleteObject(font);
        font = extra_font(dpi, selected, weight);
        old = SelectObject(dc, font);
        GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size);
        SelectObject(dc, old);
        if (size.cx <= available || selected <= minimum) break;
        --selected;
    } while (selected >= minimum);
    text_color(dc, text, rect, font, color, format);
    DeleteObject(font);
}

static int inside(RECT rect, int x, int y) {
    POINT point = {x, y};
    return PtInRect(&rect, point);
}

static RECT table_button_rect(int index, int columns, int rows, int top,
                              int width, int height, UINT dpi) {
    int margin = sx(4, dpi);
    int gap = sx(2, dpi);
    int column = index % columns;
    int row = index / columns;
    int available_width = width - margin * 2 - gap * (columns - 1);
    int available_height = height - top - margin - gap * (rows - 1);
    RECT rect;
    rect.left = margin + (available_width * column) / columns + gap * column;
    rect.right = margin + (available_width * (column + 1)) / columns + gap * column;
    rect.top = top + (available_height * row) / rows + gap * row;
    rect.bottom = top + (available_height * (row + 1)) / rows + gap * row;
    return rect;
}

static const wchar_t *scientific_label(int index) {
    static const wchar_t *const labels[] = {
        L"2ⁿᵈ", L"π", L"e", L"C", L"⌫",
        L"x²", L"1/x", L"|x|", L"exp", L"mod",
        L"²√x", L"(", L")", L"n!", L"÷",
        L"xʸ", L"7", L"8", L"9", L"×",
        L"10ˣ", L"4", L"5", L"6", L"−",
        L"log", L"1", L"2", L"3", L"+",
        L"ln", L"+/−", L"0", L",", L"="
    };
    return index >= 0 && index < (int)_countof(labels) ? labels[index] : L"";
}

static RECT scientific_option_rect(int index, int width, UINT dpi) {
    int margin = sx(8, dpi);
    int gap = sx(4, dpi);
    int available = width - margin * 2 - gap;
    RECT rect = {
        margin + (available * index) / 2 + gap * index,
        sx(183, dpi),
        margin + (available * (index + 1)) / 2 + gap * index,
        sx(221, dpi)
    };
    return rect;
}

static RECT scientific_popup_item_rect(int index, int columns, int width, UINT dpi) {
    int margin = sx(8, dpi);
    int gap = sx(3, dpi);
    int available = width - margin * 2 - gap * (columns - 1);
    int column = index % columns;
    int row = index / columns;
    RECT rect = {
        margin + (available * column) / columns + gap * column,
        sx(225 + row * 46, dpi),
        margin + (available * (column + 1)) / columns + gap * column,
        sx(268 + row * 46, dpi)
    };
    return rect;
}

static void draw_scientific_popup(HDC dc, int width, UINT dpi,
                                  int hot_id, int pressed_id) {
    static const wchar_t *const trig_labels[] = {
        L"2ⁿᵈ", L"sin", L"cos", L"tan",
        L"hyp", L"sec", L"csc", L"cot"
    };
    static const wchar_t *const function_labels[] = {
        L"|x|", L"⌊x⌋", L"⌈x⌉", L"rand", L"→dms", L"→deg"
    };
    const wchar_t *const *labels = g_scientific.popup == 1
                                       ? trig_labels : function_labels;
    int count = g_scientific.popup == 1
                    ? (int)_countof(trig_labels) : (int)_countof(function_labels);
    int columns = g_scientific.popup == 1 ? 4 : 3;
    int base = g_scientific.popup == 1 ? 70 : 80;
    HFONT font = extra_font(dpi, 10, FW_NORMAL);
    RECT panel = {sx(5, dpi), sx(221, dpi), width - sx(5, dpi), sx(320, dpi)};
    int index;
    round_color(dc, &panel, RGB(42, 42, 42), sx(5, dpi));
    for (index = 0; index < count; ++index) {
        int id = EXTRA_ID_BASE + base + index;
        RECT rect = scientific_popup_item_rect(index, columns, width, dpi);
        COLORREF fill = id == pressed_id ? RGB(78, 78, 78) :
                        id == hot_id ? RGB(69, 69, 69) : RGB(55, 55, 55);
        if (g_scientific.popup == 1 &&
            ((index == 0 && g_scientific.inverse) ||
             (index == 4 && g_scientific.hyperbolic)))
            fill = RGB(73, 85, 90);
        round_color(dc, &rect, fill, sx(4, dpi));
        text_color(dc, labels[index], rect, font, RGB(246, 246, 246),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    DeleteObject(font);
}

static void draw_scientific(HDC dc, int width, int height, UINT dpi,
                            int hot_id, int pressed_id) {
    HFONT normal = extra_font(dpi, 11, FW_NORMAL);
    HFONT small = extra_font(dpi, 9, FW_NORMAL);
    wchar_t display[128];
    wchar_t pending[128] = L"";
    RECT pending_rect = {sx(12, dpi), sx(58, dpi), width - sx(12, dpi), sx(83, dpi)};
    RECT display_rect = {sx(10, dpi), sx(78, dpi), width - sx(12, dpi), sx(151, dpi)};
    RECT angle_rect = {sx(12, dpi), sx(151, dpi), sx(75, dpi), sx(181, dpi)};
    RECT fe_rect = {sx(78, dpi), sx(151, dpi), sx(132, dpi), sx(181, dpi)};
    int index;
    if (g_scientific.has_accumulator && g_scientific.pending) {
        wchar_t number[96];
        wchar_t operation[12];
        format_double_wide(g_scientific.accumulator, number, _countof(number));
        if (g_scientific.pending == '%') copy_wide(operation, _countof(operation), L"mod");
        else {
            operation[0] = g_scientific.pending == '*' ? L'×' :
                           g_scientific.pending == '/' ? L'÷' :
                           g_scientific.pending == '-' ? L'−' :
                           (wchar_t)g_scientific.pending;
            operation[1] = L'\0';
        }
        _snwprintf(pending, _countof(pending), L"%ls %ls", number, operation);
        pending[_countof(pending) - 1] = L'\0';
    }
    format_double_wide(scientific_value(), display, _countof(display));
    if (g_scientific.error) copy_wide(display, _countof(display), L"Invalid input");
    text_color(dc, pending, pending_rect, small, RGB(170, 175, 176),
               DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    fit_text(dc, display, display_rect, dpi, 32, 11, FW_SEMIBOLD,
             RGB(246, 246, 246), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    text_color(dc, g_scientific.angle_mode == 0 ? L"DEG" :
                   g_scientific.angle_mode == 1 ? L"RAD" : L"GRAD",
               angle_rect, small,
               hot_id == EXTRA_ID_BASE + 60 ? RGB(246, 246, 246)
                                             : RGB(205, 208, 209),
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    text_color(dc, L"F-E", fe_rect, small,
               g_scientific.f_e ? RGB(156, 198, 217) :
               hot_id == EXTRA_ID_BASE + 61 ? RGB(246, 246, 246)
                                             : RGB(205, 208, 209),
               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    for (index = 0; index < 2; ++index) {
        int id = EXTRA_ID_BASE + 62 + index;
        RECT rect = scientific_option_rect(index, width, dpi);
        COLORREF fill = id == pressed_id ? RGB(78, 78, 78) :
                        id == hot_id ? RGB(62, 62, 62) :
                        g_scientific.popup == index + 1 ? RGB(62, 70, 73)
                                                       : RGB(50, 50, 50);
        round_color(dc, &rect, fill, sx(4, dpi));
        text_color(dc, index == 0 ? L"△  Trigonometry   ⌄"
                                  : L"ƒ  Function   ⌄",
                   rect, small, RGB(246, 246, 246),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (index = 0; index < 35; ++index) {
        int id = EXTRA_ID_BASE + index;
        RECT rect = table_button_rect(index, 5, 7, sx(225, dpi), width, height, dpi);
        COLORREF fill = (index == 34) ? RGB(156, 198, 217) :
                        ((index >= 16 && index <= 18) ||
                         (index >= 21 && index <= 23) ||
                         (index >= 26 && index <= 28) ||
                         index == 31 || index == 32 || index == 33)
                            ? RGB(59, 59, 59) : RGB(50, 50, 50);
        COLORREF color = index == 34 ? RGB(24, 35, 40) : RGB(246, 246, 246);
        if (id == pressed_id) fill = RGB(78, 78, 78);
        else if (id == hot_id) fill = index == 34 ? RGB(176, 213, 230) : RGB(69, 69, 69);
        if (index == 0 && g_scientific.inverse) fill = RGB(73, 85, 90);
        round_color(dc, &rect, fill, sx(5, dpi));
        text_color(dc, scientific_label(index), rect, normal, color,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (g_scientific.popup)
        draw_scientific_popup(dc, width, dpi, hot_id, pressed_id);
    DeleteObject(small);
    DeleteObject(normal);
}

static void format_programmer(uint64_t value, int base, wchar_t *output, size_t capacity) {
    if (base == 16) _snwprintf(output, capacity, L"%llX", (unsigned long long)value);
    else if (base == 10) _snwprintf(output, capacity, L"%llu", (unsigned long long)value);
    else if (base == 8) _snwprintf(output, capacity, L"%llo", (unsigned long long)value);
    else {
        wchar_t reverse[80];
        size_t count = 0;
        size_t index;
        do {
            reverse[count++] = (wchar_t)(L'0' + (value & 1ULL));
            value >>= 1;
        } while (value && count < _countof(reverse));
        if (count + 1 > capacity) count = capacity - 1;
        for (index = 0; index < count; ++index) output[index] = reverse[count - 1 - index];
        output[count] = L'\0';
        return;
    }
    output[capacity - 1] = L'\0';
}

static const wchar_t *programmer_label(int index) {
    static const wchar_t *const labels[] = {
        L"A", L"<<", L">>", L"C", L"⌫",
        L"B", L"(", L")", L"%", L"÷",
        L"C", L"7", L"8", L"9", L"×",
        L"D", L"4", L"5", L"6", L"−",
        L"E", L"1", L"2", L"3", L"+",
        L"F", L"+/−", L"0", L"", L"="
    };
    return index >= 0 && index < (int)_countof(labels) ? labels[index] : L"";
}

static RECT programmer_base_rect(int index, int width, UINT dpi) {
    RECT rect = {sx(8, dpi), sx(103 + index * 22, dpi),
                 width - sx(8, dpi), sx(125 + index * 22, dpi)};
    return rect;
}

static RECT programmer_tool_rect(int index, int width, UINT dpi) {
    static const int proportions[] = {0, 14, 39, 71, 100};
    int margin = sx(7, dpi);
    int available = width - margin * 2;
    RECT rect = {
        margin + available * proportions[index] / 100,
        sx(193, dpi),
        margin + available * proportions[index + 1] / 100 - sx(2, dpi),
        sx(233, dpi)
    };
    return rect;
}

static RECT programmer_popup_item_rect(int index, int width, UINT dpi) {
    RECT rect;
    if (g_programmer.popup == 1) {
        int margin = sx(8, dpi);
        int gap = sx(3, dpi);
        int available = width - margin * 2 - gap * 2;
        int column = index % 3;
        int row = index / 3;
        rect.left = margin + available * column / 3 + gap * column;
        rect.right = margin + available * (column + 1) / 3 + gap * column;
        rect.top = sx(238 + row * 49, dpi);
        rect.bottom = sx(284 + row * 49, dpi);
    } else {
        rect.left = sx(8, dpi);
        rect.right = width - sx(8, dpi);
        rect.top = sx(238 + index * 46, dpi);
        rect.bottom = sx(281 + index * 46, dpi);
    }
    return rect;
}

static RECT programmer_bit_rect(int bit, int width, UINT dpi) {
    int display_index = g_programmer.bits - 1 - bit;
    int row = display_index / 16;
    int column = display_index % 16;
    int margin = sx(8, dpi);
    int group_gap = sx(5, dpi);
    int available = width - margin * 2 - group_gap * 3;
    int group = column / 4;
    int within = column % 4;
    int cell = available / 16;
    RECT rect = {
        margin + group * (cell * 4 + group_gap) + within * cell,
        sx(250 + row * 70, dpi),
        margin + group * (cell * 4 + group_gap) + (within + 1) * cell,
        sx(282 + row * 70, dpi)
    };
    return rect;
}

static int programmer_digit_for_button(int index) {
    if (index == 0) return 10;
    if (index == 5) return 11;
    if (index == 10) return 12;
    if (index == 15) return 13;
    if (index == 20) return 14;
    if (index == 25) return 15;
    if (index >= 11 && index <= 13) return 7 + index - 11;
    if (index >= 16 && index <= 18) return 4 + index - 16;
    if (index >= 21 && index <= 23) return 1 + index - 21;
    if (index == 27) return 0;
    return -1;
}

static void draw_programmer_popup(HDC dc, int width, UINT dpi,
                                  int hot_id, int pressed_id) {
    static const wchar_t *const bitwise[] = {
        L"AND", L"OR", L"NOT", L"NAND", L"NOR", L"XOR"
    };
    static const wchar_t *const shifts[] = {
        L"Arithmetic shift", L"Logical shift",
        L"Rotate circular shift", L"Rotate through carry circular shift"
    };
    int count = g_programmer.popup == 1 ? 6 : 4;
    int base = g_programmer.popup == 1 ? 170 : 180;
    HFONT normal = extra_font(dpi, 10, FW_NORMAL);
    HFONT small = extra_font(dpi, 9, FW_NORMAL);
    RECT panel = {sx(5, dpi), sx(234, dpi), width - sx(5, dpi),
                  sx(g_programmer.popup == 1 ? 338 : 426, dpi)};
    int index;
    round_color(dc, &panel, RGB(42, 42, 42), sx(5, dpi));
    for (index = 0; index < count; ++index) {
        int id = EXTRA_ID_BASE + base + index;
        RECT rect = programmer_popup_item_rect(index, width, dpi);
        COLORREF fill = id == pressed_id ? RGB(78, 78, 78) :
                        id == hot_id ? RGB(69, 69, 69) : RGB(55, 55, 55);
        wchar_t shift_label[80];
        const wchar_t *label;
        if (g_programmer.popup == 2) {
            _snwprintf(shift_label, _countof(shift_label), L"%ls%ls",
                       index == g_programmer.shift_mode ? L"●  " : L"○  ",
                       shifts[index]);
            shift_label[_countof(shift_label) - 1] = L'\0';
            label = shift_label;
        } else {
            label = bitwise[index];
        }
        round_color(dc, &rect, fill, sx(4, dpi));
        text_color(dc, label, rect, g_programmer.popup == 2 ? small : normal,
                   RGB(246, 246, 246),
                   (g_programmer.popup == 2 ? DT_LEFT : DT_CENTER) |
                   DT_VCENTER | DT_SINGLELINE);
    }
    DeleteObject(small);
    DeleteObject(normal);
}

static void draw_programmer_bits(HDC dc, int width, UINT dpi,
                                 int hot_id, int pressed_id) {
    HFONT bit_font = extra_font(dpi, 12, FW_SEMIBOLD);
    HFONT index_font = extra_font(dpi, 7, FW_NORMAL);
    int bit;
    for (bit = 0; bit < g_programmer.bits; ++bit) {
        int id = EXTRA_ID_BASE + 700 + bit;
        RECT rect = programmer_bit_rect(bit, width, dpi);
        wchar_t value[2] = {(g_programmer.value >> bit) & 1ULL ? L'1' : L'0', L'\0'};
        COLORREF color = id == hot_id || id == pressed_id
                             ? RGB(156, 198, 217) : RGB(246, 246, 246);
        text_color(dc, value, rect, bit_font, color,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (bit % 4 == 0) {
            wchar_t index_text[8];
            RECT label = rect;
            label.left -= sx(38, dpi);
            label.right = rect.right;
            label.top = rect.bottom;
            label.bottom = rect.bottom + sx(18, dpi);
            _snwprintf(index_text, _countof(index_text), L"%d", bit);
            text_color(dc, index_text, label, index_font, RGB(155, 160, 161),
                       DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }
    DeleteObject(index_font);
    DeleteObject(bit_font);
}

static void draw_programmer(HDC dc, int width, int height, UINT dpi,
                            int hot_id, int pressed_id) {
    static const wchar_t *const bases[] = {L"HEX", L"DEC", L"OCT", L"BIN"};
    static const int base_values[] = {16, 10, 8, 2};
    HFONT normal = extra_font(dpi, 10, FW_NORMAL);
    HFONT small = extra_font(dpi, 8, FW_NORMAL);
    wchar_t text[96];
    int index;
    {
        RECT display = {sx(10, dpi), sx(52, dpi), width - sx(10, dpi), sx(103, dpi)};
        format_programmer(g_programmer.value, g_programmer.base, text, _countof(text));
        fit_text(dc, g_programmer.error ? L"Invalid operation" : text, display,
                 dpi, 25, 10, FW_SEMIBOLD, RGB(246, 246, 246),
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
    {
        const int shown_bases[] = {16, 10, 8, 2};
        for (index = 0; index < 4; ++index) {
            int id = EXTRA_ID_BASE + 100 + index;
            RECT row = programmer_base_rect(index, width, dpi);
            RECT name_rect = row;
            RECT value_rect = row;
            name_rect.left += sx(4, dpi);
            name_rect.right = name_rect.left + sx(42, dpi);
            value_rect.left += sx(48, dpi);
            value_rect.right -= sx(4, dpi);
            format_programmer(g_programmer.value, shown_bases[index], text, _countof(text));
            if (id == hot_id) round_color(dc, &row, RGB(48, 48, 48), sx(3, dpi));
            if (g_programmer.base == base_values[index]) {
                RECT marker = {row.left, row.top + sx(2, dpi),
                               row.left + sx(3, dpi), row.bottom - sx(2, dpi)};
                round_color(dc, &marker, RGB(156, 198, 217), sx(2, dpi));
            }
            text_color(dc, bases[index], name_rect, small,
                       g_programmer.base == base_values[index]
                           ? RGB(156, 198, 217) : RGB(220, 222, 223),
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            fit_text(dc, g_programmer.error ? L"Invalid operation" : text, value_rect,
                     dpi, 11, 7, FW_NORMAL, RGB(246, 246, 246),
                     DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }
    for (index = 0; index < 4; ++index) {
        int id = EXTRA_ID_BASE + 160 + index;
        RECT rect = programmer_tool_rect(index, width, dpi);
        COLORREF fill = id == pressed_id ? RGB(78, 78, 78) :
                        id == hot_id ? RGB(62, 62, 62) :
                        ((index == 0 && g_programmer.bit_keypad) ||
                         (index == 1 && g_programmer.popup == 1) ||
                         (index == 2 && g_programmer.popup == 2))
                            ? RGB(62, 70, 73) : RGB(43, 43, 43);
        const wchar_t *label = index == 1 ? L"Bitwise⌄" :
                               index == 2 ? L"Bit shift⌄" :
                               g_programmer.bits == 64 ? L"QWORD" :
                               g_programmer.bits == 32 ? L"DWORD" :
                               g_programmer.bits == 16 ? L"WORD" : L"BYTE";
        round_color(dc, &rect, fill, sx(4, dpi));
        if (index == 0) {
            HBRUSH dot = CreateSolidBrush(RGB(246, 246, 246));
            HGDIOBJ old_brush = SelectObject(dc, dot);
            HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
            int column;
            int row;
            int center_x = (rect.left + rect.right) / 2;
            int center_y = (rect.top + rect.bottom) / 2;
            int radius = sx(2, dpi);
            int spacing = sx(7, dpi);
            for (row = -1; row <= 1; ++row) {
                for (column = -1; column <= 1; ++column) {
                    int x = center_x + column * spacing;
                    int y = center_y + row * spacing;
                    Ellipse(dc, x - radius, y - radius, x + radius + 1, y + radius + 1);
                }
            }
            SelectObject(dc, old_pen);
            SelectObject(dc, old_brush);
            DeleteObject(dot);
        } else {
            text_color(dc, label, rect, small, RGB(246, 246, 246),
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    if (g_programmer.bit_keypad) {
        draw_programmer_bits(dc, width, dpi, hot_id, pressed_id);
    } else {
    for (index = 0; index < 30; ++index) {
        int id = EXTRA_ID_BASE + 120 + index;
        RECT rect = table_button_rect(index, 5, 6, sx(237, dpi), width, height, dpi);
        int digit = programmer_digit_for_button(index);
        int digit_disabled = (digit >= g_programmer.base && digit >= 0) ||
                             index == 28;
        COLORREF fill = index == 29 ? RGB(156, 198, 217) :
                        (digit >= 0)
                            ? RGB(59, 59, 59) : RGB(50, 50, 50);
        COLORREF color = digit_disabled ? RGB(103, 108, 109) :
                         index == 29 ? RGB(24, 35, 40) : RGB(246, 246, 246);
        if (!digit_disabled && id == pressed_id) fill = RGB(78, 78, 78);
        else if (!digit_disabled && id == hot_id)
            fill = index == 29 ? RGB(176, 213, 230) : RGB(69, 69, 69);
        round_color(dc, &rect, fill, sx(4, dpi));
        text_color(dc, programmer_label(index), rect, small, color,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    }
    if (g_programmer.popup)
        draw_programmer_popup(dc, width, dpi, hot_id, pressed_id);
    DeleteObject(small);
    DeleteObject(normal);
}

static void format_date(const SYSTEMTIME *date, wchar_t *output, size_t capacity) {
    _snwprintf(output, capacity, L"%04u-%02u-%02u",
               date->wYear, date->wMonth, date->wDay);
    output[capacity - 1] = L'\0';
}

static RECT date_tab_rect(int index, int width, UINT dpi) {
    int margin = sx(8, dpi);
    int gap = sx(4, dpi);
    int available = width - margin * 2 - gap;
    RECT rect = {
        margin + (available * index) / 2 + gap * index,
        sx(61, dpi),
        margin + (available * (index + 1)) / 2 + gap * index,
        sx(98, dpi)
    };
    return rect;
}

static RECT date_field_rect(int index, int width, UINT dpi) {
    int top = index == 0 ? sx(126, dpi) : sx(224, dpi);
    RECT rect = {sx(12, dpi), top, width - sx(12, dpi), top + sx(47, dpi)};
    return rect;
}

static RECT date_result_rect(int width, int height, UINT dpi) {
    RECT rect = {sx(12, dpi), sx(g_date.add_mode ? 361 : 310, dpi),
                 width - sx(12, dpi), height - sx(18, dpi)};
    return rect;
}

static RECT date_calendar_panel_rect(int width, int height, UINT dpi) {
    RECT rect = {sx(4, dpi), sx(105, dpi), width - sx(4, dpi),
                 height - sx(8, dpi)};
    return rect;
}

static RECT date_calendar_arrow_rect(int next, int width, int height, UINT dpi) {
    RECT panel = date_calendar_panel_rect(width, height, dpi);
    RECT rect = {
        next ? panel.right - sx(53, dpi) : panel.left + sx(8, dpi),
        panel.top + sx(8, dpi),
        next ? panel.right - sx(8, dpi) : panel.left + sx(53, dpi),
        panel.top + sx(48, dpi)
    };
    return rect;
}

static RECT date_calendar_cell_rect(int cell, int width, int height, UINT dpi) {
    RECT panel = date_calendar_panel_rect(width, height, dpi);
    int margin = sx(7, dpi);
    int gap = sx(2, dpi);
    int grid_top = panel.top + sx(86, dpi);
    int available_width = panel.right - panel.left - margin * 2 - gap * 6;
    int available_height = panel.bottom - grid_top - sx(8, dpi) - gap * 5;
    int column = cell % 7;
    int row = cell / 7;
    RECT rect = {
        panel.left + margin + (available_width * column) / 7 + gap * column,
        grid_top + (available_height * row) / 6 + gap * row,
        panel.left + margin + (available_width * (column + 1)) / 7 + gap * column,
        grid_top + (available_height * (row + 1)) / 6 + gap * row
    };
    return rect;
}

static void format_date_long(const SYSTEMTIME *date, wchar_t *output,
                             size_t capacity) {
    if (!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, date,
                         L"d MMMM yyyy", output, (int)capacity, NULL))
        format_date(date, output, capacity);
}

static int date_calendar_first_weekday(void) {
    SYSTEMTIME first;
    SYSTEMTIME normalized;
    FILETIME checked;
    ZeroMemory(&first, sizeof(first));
    ZeroMemory(&normalized, sizeof(normalized));
    first.wYear = (WORD)g_date.calendar_year;
    first.wMonth = (WORD)g_date.calendar_month;
    first.wDay = 1;
    if (!SystemTimeToFileTime(&first, &checked)) return 0;
    if (!FileTimeToSystemTime(&checked, &normalized)) return 0;
    return ((int)normalized.wDayOfWeek + 6) % 7;
}

static SYSTEMTIME date_calendar_cell_value(int cell) {
    SYSTEMTIME value;
    int offset = cell - date_calendar_first_weekday();
    ZeroMemory(&value, sizeof(value));
    value.wYear = (WORD)g_date.calendar_year;
    value.wMonth = (WORD)g_date.calendar_month;
    value.wDay = 1;
    add_days(&value, offset);
    return value;
}

static void draw_small_calendar_icon(HDC dc, RECT rect, UINT dpi, COLORREF color) {
    int size = sx(16, dpi);
    RECT icon = {rect.right - sx(28, dpi),
                 (rect.top + rect.bottom - size) / 2,
                 rect.right - sx(12, dpi),
                 (rect.top + rect.bottom + size) / 2};
    HPEN pen = CreatePen(PS_SOLID, sx(1, dpi), color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, icon.left, icon.top + sx(2, dpi), icon.right, icon.bottom);
    MoveToEx(dc, icon.left, icon.top + sx(6, dpi), NULL);
    LineTo(dc, icon.right, icon.top + sx(6, dpi));
    MoveToEx(dc, icon.left + sx(4, dpi), icon.top, NULL);
    LineTo(dc, icon.left + sx(4, dpi), icon.top + sx(4, dpi));
    MoveToEx(dc, icon.right - sx(4, dpi), icon.top, NULL);
    LineTo(dc, icon.right - sx(4, dpi), icon.top + sx(4, dpi));
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static void draw_date_field(HDC dc, int index, int width, UINT dpi,
                            int hot_id, int pressed_id) {
    wchar_t date_text[80];
    RECT rect = date_field_rect(index, width, dpi);
    RECT text_rect = rect;
    int id = EXTRA_ID_BASE + 210 + index;
    const SYSTEMTIME *date = index ? &g_date.second : &g_date.first;
    COLORREF fill = id == pressed_id ? RGB(78, 78, 78) :
                    id == hot_id ? RGB(69, 69, 69) : RGB(43, 43, 43);
    round_color(dc, &rect, fill, sx(5, dpi));
    text_rect.left += sx(10, dpi);
    text_rect.right -= sx(38, dpi);
    format_date_long(date, date_text, _countof(date_text));
    fit_text(dc, date_text, text_rect, dpi, 12, 8, FW_NORMAL,
             RGB(246, 246, 246), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_small_calendar_icon(dc, rect, dpi, RGB(210, 213, 214));
}

static void draw_date_calendar(HDC dc, int width, int height, UINT dpi,
                               int hot_id, int pressed_id) {
    static const wchar_t *const weekdays[] = {
        L"Mo", L"Tu", L"We", L"Th", L"Fr", L"Sa", L"Su"
    };
    RECT panel = date_calendar_panel_rect(width, height, dpi);
    RECT header = {panel.left + sx(58, dpi), panel.top + sx(8, dpi),
                   panel.right - sx(58, dpi), panel.top + sx(48, dpi)};
    wchar_t month[80];
    SYSTEMTIME month_date;
    HFONT normal = extra_font(dpi, 10, FW_NORMAL);
    HFONT title = extra_font(dpi, 12, FW_SEMIBOLD);
    int index;
    round_color(dc, &panel, RGB(43, 43, 43), sx(7, dpi));
    ZeroMemory(&month_date, sizeof(month_date));
    month_date.wYear = (WORD)g_date.calendar_year;
    month_date.wMonth = (WORD)g_date.calendar_month;
    month_date.wDay = 1;
    if (!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &month_date,
                         L"MMMM yyyy", month, (int)_countof(month), NULL))
        _snwprintf(month, _countof(month), L"%d-%02d",
                   g_date.calendar_year, g_date.calendar_month);
    text_color(dc, month, header, title, RGB(246, 246, 246),
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    for (index = 0; index < 2; ++index) {
        int id = EXTRA_ID_BASE + 220 + index;
        RECT rect = date_calendar_arrow_rect(index, width, height, dpi);
        COLORREF fill = id == pressed_id ? RGB(78, 78, 78) :
                        id == hot_id ? RGB(69, 69, 69) : RGB(50, 50, 50);
        round_color(dc, &rect, fill, sx(4, dpi));
        text_color(dc, index ? L"›" : L"‹", rect, title, RGB(246, 246, 246),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (index = 0; index < 7; ++index) {
        RECT cell = date_calendar_cell_rect(index, width, height, dpi);
        cell.top = panel.top + sx(50, dpi);
        cell.bottom = panel.top + sx(84, dpi);
        text_color(dc, weekdays[index], cell, normal, RGB(220, 222, 223),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (index = 0; index < 42; ++index) {
        SYSTEMTIME value = date_calendar_cell_value(index);
        const SYSTEMTIME *selected = g_date.calendar_target == 2
                                         ? &g_date.second : &g_date.first;
        int id = EXTRA_ID_BASE + 222 + index;
        RECT cell = date_calendar_cell_rect(index, width, height, dpi);
        wchar_t day[8];
        COLORREF color = value.wMonth == g_date.calendar_month
                             ? RGB(246, 246, 246) : RGB(145, 149, 150);
        if (value.wYear == selected->wYear &&
            value.wMonth == selected->wMonth &&
            value.wDay == selected->wDay) {
            round_color(dc, &cell, RGB(156, 198, 217), sx(18, dpi));
            color = RGB(24, 35, 40);
        } else if (id == pressed_id || id == hot_id) {
            round_color(dc, &cell,
                        id == pressed_id ? RGB(78, 78, 78) : RGB(69, 69, 69),
                        sx(18, dpi));
        }
        _snwprintf(day, _countof(day), L"%u", value.wDay);
        text_color(dc, day, cell, normal, color,
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    DeleteObject(title);
    DeleteObject(normal);
}

static void draw_date(HDC dc, int width, int height, UINT dpi,
                      int hot_id, int pressed_id) {
    HFONT normal = extra_font(dpi, 11, FW_NORMAL);
    HFONT small = extra_font(dpi, 9, FW_NORMAL);
    int index;
    for (index = 0; index < 2; ++index) {
        int id = EXTRA_ID_BASE + 200 + index;
        RECT rect = date_tab_rect(index, width, dpi);
        COLORREF fill = g_date.add_mode == index ? RGB(73, 85, 90) : RGB(50, 50, 50);
        if (id == hot_id) fill = RGB(69, 69, 69);
        round_color(dc, &rect, fill, sx(5, dpi));
        text_color(dc, index == 0 ? L"Difference" : L"Add or subtract",
                   rect, small, RGB(246, 246, 246),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    {
        RECT label = {sx(18, dpi), sx(103, dpi), width - sx(18, dpi), sx(125, dpi)};
        text_color(dc, g_date.add_mode ? L"Starting date" : L"From",
                   label, small, RGB(170, 175, 176),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        draw_date_field(dc, 0, width, dpi, hot_id, pressed_id);
    }
    if (!g_date.add_mode) {
        wchar_t result[96];
        long long days = date_difference_days(&g_date.first, &g_date.second);
        RECT to_label = {sx(18, dpi), sx(201, dpi), width - sx(18, dpi), sx(223, dpi)};
        RECT difference_label = {sx(18, dpi), sx(278, dpi),
                                 width - sx(18, dpi), sx(307, dpi)};
        RECT result_rect = date_result_rect(width, height, dpi);
        text_color(dc, L"To", to_label, small, RGB(170, 175, 176),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        draw_date_field(dc, 1, width, dpi, hot_id, pressed_id);
        text_color(dc, L"Difference", difference_label, small, RGB(170, 175, 176),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        _snwprintf(result, _countof(result), L"%lld day%ls apart",
                   days, days == 1 ? L"" : L"s");
        fit_text(dc, result, result_rect, dpi, 25, 12, FW_SEMIBOLD,
                 RGB(246, 246, 246), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        SYSTEMTIME result = g_date.first;
        wchar_t result_text[32];
        wchar_t amount[64];
        RECT amount_rect = {sx(12, dpi), sx(201, dpi),
                            width - sx(12, dpi), sx(260, dpi)};
        RECT result_rect = date_result_rect(width, height, dpi);
        adjust_date(&result, g_date.add_unit,
                    (g_date.subtract ? -1 : 1) * g_date.add_amount);
        format_date(&result, result_text, _countof(result_text));
        _snwprintf(amount, _countof(amount), L"%ls %d %ls",
                   g_date.subtract ? L"Subtract" : L"Add", g_date.add_amount,
                   g_date.add_unit == 0 ? L"days" :
                   g_date.add_unit == 1 ? L"months" : L"years");
        text_color(dc, amount, amount_rect, normal, RGB(246, 246, 246),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        for (index = 0; index < 6; ++index) {
            static const wchar_t *const labels[] = {
                L"− amount", L"+ amount", L"Add/Subtract",
                L"Days", L"Months", L"Years"
            };
            int id = EXTRA_ID_BASE + 240 + index;
            RECT rect = table_button_rect(index, 3, 2, sx(261, dpi),
                                          width, sx(347, dpi), dpi);
            COLORREF fill = (index >= 3 && g_date.add_unit == index - 3) ||
                            (index == 2 && g_date.subtract)
                                ? RGB(73, 85, 90) : RGB(50, 50, 50);
            if (id == hot_id) fill = RGB(69, 69, 69);
            round_color(dc, &rect, fill, sx(4, dpi));
            text_color(dc, labels[index], rect, small, RGB(246, 246, 246),
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        fit_text(dc, result_text, result_rect, dpi, 27, 13, FW_SEMIBOLD,
                 RGB(246, 246, 246), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (g_date.calendar_target)
        draw_date_calendar(dc, width, height, dpi, hot_id, pressed_id);
    DeleteObject(small);
    DeleteObject(normal);
}

static RECT converter_card_rect(int output, int width, UINT dpi) {
    RECT rect = {sx(8, dpi), sx(output ? 205 : 68, dpi),
                 width - sx(8, dpi), sx(output ? 318 : 181, dpi)};
    return rect;
}

static RECT converter_unit_rect(int output, int width, UINT dpi) {
    RECT card = converter_card_rect(output, width, dpi);
    RECT rect = {card.left + sx(7, dpi), card.bottom - sx(39, dpi),
                 card.right - sx(7, dpi), card.bottom - sx(6, dpi)};
    return rect;
}

static const wchar_t *converter_unit_label(ExtraMode mode, int index,
                                            wchar_t *buffer, size_t capacity) {
    int count;
    const UnitDef *table;
    if (mode == MODE_CURRENCY)
        return index >= 0 && index < g_currency_count ? g_currency[index].label : L"No rates";
    table = unit_table(mode, &count);
    if (!table || index < 0 || index >= count) return L"Choose a unit";
    if (table[index].symbol && *table[index].symbol)
        _snwprintf(buffer, capacity, L"%ls  (%ls)", table[index].name, table[index].symbol);
    else
        _snwprintf(buffer, capacity, L"%ls", table[index].name);
    buffer[capacity - 1] = L'\0';
    return buffer;
}

static void format_currency_amount(double value, int index, wchar_t *output,
                                   size_t capacity) {
    wchar_t number[128];
    if (index < 0 || index >= g_currency_count) {
        format_double_wide(value, output, capacity);
        return;
    }
    format_double_wide(value, number, _countof(number));
    if (g_currency[index].symbol_prefix)
        _snwprintf(output, capacity, L"%ls %ls", g_currency[index].symbol, number);
    else
        _snwprintf(output, capacity, L"%ls %ls", number, g_currency[index].symbol);
    output[capacity - 1] = L'\0';
}

static RECT currency_source_rect(int width, UINT dpi) {
    RECT rect = {sx(10, dpi), sx(311, dpi), width - sx(10, dpi), sx(326, dpi)};
    return rect;
}

static void draw_chevron(HDC dc, RECT rect, UINT dpi, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, sx(1, dpi), color);
    HGDIOBJ old = SelectObject(dc, pen);
    int center_x = rect.right - sx(17, dpi);
    int center_y = (rect.top + rect.bottom) / 2;
    MoveToEx(dc, center_x - sx(4, dpi), center_y - sx(2, dpi), NULL);
    LineTo(dc, center_x, center_y + sx(2, dpi));
    LineTo(dc, center_x + sx(4, dpi), center_y - sx(2, dpi));
    SelectObject(dc, old);
    DeleteObject(pen);
}

static int text_starts_with_case_insensitive(const wchar_t *text,
                                             const wchar_t *prefix) {
    if (!text || !prefix || !*prefix) return 0;
    while (*prefix) {
        if (!*text || towlower(*text) != towlower(*prefix)) return 0;
        ++text;
        ++prefix;
    }
    return 1;
}

static int text_contains_case_insensitive(const wchar_t *text,
                                          const wchar_t *needle) {
    if (!text || !needle || !*needle) return 0;
    while (*text) {
        if (text_starts_with_case_insensitive(text, needle)) return 1;
        ++text;
    }
    return 0;
}

static int picker_item_matches(int index, const wchar_t *search, int prefix_only) {
    if (index < 0 || index >= extras_unit_count(g_mode) || !search || !*search)
        return 0;
    if (g_mode == MODE_CURRENCY) {
        const CurrencyRate *currency = &g_currency[index];
        if (prefix_only) {
            return text_starts_with_case_insensitive(currency->location, search) ||
                   text_starts_with_case_insensitive(currency->name, search) ||
                   text_starts_with_case_insensitive(currency->code, search);
        }
        return text_contains_case_insensitive(currency->label, search) ||
               text_contains_case_insensitive(currency->code, search);
    }
    return prefix_only
               ? text_starts_with_case_insensitive(extras_unit_name(g_mode, index), search)
               : text_contains_case_insensitive(extras_unit_name(g_mode, index), search);
}

static void picker_jump_to_search(void) {
    int count = extras_unit_count(g_mode);
    int visible = count < EXTRA_MAX_PICKER_ROWS ? count : EXTRA_MAX_PICKER_ROWS;
    int index;
    int match = -1;
    if (!g_converter.picker_search[0]) return;
    for (index = 0; index < count; ++index) {
        if (picker_item_matches(index, g_converter.picker_search, 1)) {
            match = index;
            break;
        }
    }
    if (match < 0) {
        for (index = 0; index < count; ++index) {
            if (picker_item_matches(index, g_converter.picker_search, 0)) {
                match = index;
                break;
            }
        }
    }
    if (match >= 0) {
        int maximum = count > visible ? count - visible : 0;
        g_converter.picker_scroll = match;
        if (g_converter.picker_scroll > maximum)
            g_converter.picker_scroll = maximum;
    }
}

static void draw_converter_picker(HDC dc, int width, int height, UINT dpi,
                                  int hot_id, int pressed_id) {
    int count = extras_unit_count(g_mode);
    int visible = count < EXTRA_MAX_PICKER_ROWS ? count : EXTRA_MAX_PICKER_ROWS;
    int row_height = sx(45, dpi);
    int panel_height = visible * row_height + sx(58, dpi);
    int top = sx(76, dpi);
    int index;
    RECT panel = {sx(9, dpi), top, width - sx(9, dpi),
                  top + panel_height < height - sx(8, dpi)
                      ? top + panel_height : height - sx(8, dpi)};
    HFONT normal = extra_font(dpi, 11, FW_NORMAL);
    HFONT small = extra_font(dpi, 9, FW_NORMAL);
    fill_color(dc, &panel, RGB(39, 44, 46));
    {
        RECT heading = {panel.left + sx(12, dpi), panel.top,
                        panel.right - sx(12, dpi), panel.top + sx(48, dpi)};
        wchar_t heading_text[128];
        if (g_converter.picker_search[0]) {
            _snwprintf(heading_text, _countof(heading_text), L"%ls  ·  Type to jump: %ls",
                       g_converter.picker_target ? L"Convert to" : L"Convert from",
                       g_converter.picker_search);
        } else {
            _snwprintf(heading_text, _countof(heading_text), L"%ls  ·  Type to jump",
                       g_converter.picker_target ? L"Convert to" : L"Convert from");
        }
        heading_text[_countof(heading_text) - 1] = L'\0';
        fit_text(dc, heading_text, heading, dpi, 10, 7, FW_NORMAL,
                 RGB(246, 246, 246),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    if (!count) {
        RECT empty = {panel.left + sx(12, dpi), panel.top + sx(55, dpi),
                      panel.right - sx(12, dpi), panel.bottom - sx(12, dpi)};
        text_color(dc, L"Currency rates aren't available yet.", empty, normal,
                   RGB(170, 175, 176), DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
    if (g_converter.picker_scroll < 0) g_converter.picker_scroll = 0;
    if (g_converter.picker_scroll > count - visible)
        g_converter.picker_scroll = count > visible ? count - visible : 0;
    for (index = 0; index < visible; ++index) {
        int actual = g_converter.picker_scroll + index;
        int id = EXTRA_ID_BASE + 500 + index;
        wchar_t label[128];
        RECT row = {panel.left + sx(5, dpi), panel.top + sx(48, dpi) + index * row_height,
                    panel.right - sx(5, dpi),
                    panel.top + sx(48, dpi) + (index + 1) * row_height - sx(2, dpi)};
        COLORREF fill = id == pressed_id ? RGB(78, 78, 78) :
                        id == hot_id ? RGB(69, 69, 69) : RGB(50, 50, 50);
        int selected = g_converter.picker_target
                           ? g_converter.to_index[g_mode]
                           : g_converter.from_index[g_mode];
        if (actual == selected) fill = RGB(73, 85, 90);
        round_color(dc, &row, fill, sx(4, dpi));
        if (g_mode == MODE_CURRENCY) {
            _snwprintf(label, _countof(label), L"%ls   (%ls)",
                       g_currency[actual].label, g_currency[actual].code);
        } else {
            copy_wide(label, _countof(label), extras_unit_name(g_mode, actual));
            int unit_count;
            const UnitDef *table = unit_table(g_mode, &unit_count);
            if (table && actual < unit_count && table[actual].symbol && *table[actual].symbol)
                _snwprintf(label, _countof(label), L"%ls   %ls",
                           table[actual].name, table[actual].symbol);
        }
        label[_countof(label) - 1] = L'\0';
        fit_text(dc, label, row, dpi, 10, 7, FW_NORMAL, RGB(246, 246, 246),
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    DeleteObject(small);
    DeleteObject(normal);
}

static void draw_converter(HDC dc, int width, int height, UINT dpi,
                           int hot_id, int pressed_id) {
    static const wchar_t *const keypad[] = {
        L"7", L"8", L"9", L"⌫",
        L"4", L"5", L"6", L"CE",
        L"1", L"2", L"3", L"⇄",
        L"", L"0", L",", L""
    };
    HFONT normal = extra_font(dpi, 11, FW_NORMAL);
    HFONT small = extra_font(dpi, 9, FW_NORMAL);
    wchar_t input[128];
    wchar_t output[128];
    wchar_t unit_text[160];
    wchar_t rate_text[192] = L"";
    double input_value = strtod(g_converter.input[g_mode], NULL);
    double converted = extras_convert_value(
        g_mode, g_converter.from_index[g_mode], g_converter.to_index[g_mode], input_value);
    RECT from_card = converter_card_rect(0, width, dpi);
    RECT to_card = converter_card_rect(1, width, dpi);
    RECT from_unit = converter_unit_rect(0, width, dpi);
    RECT to_unit = converter_unit_rect(1, width, dpi);
    RECT from_value = from_card;
    RECT to_value = to_card;
    int index;
    int keypad_height = g_mode == MODE_CURRENCY ? height - sx(18, dpi) : height;
    if (g_mode == MODE_CURRENCY) {
        format_currency_amount(input_value, g_converter.from_index[g_mode],
                               input, _countof(input));
        format_currency_amount(converted, g_converter.to_index[g_mode],
                               output, _countof(output));
        if (g_currency_loaded &&
            g_converter.from_index[g_mode] >= 0 &&
            g_converter.from_index[g_mode] < g_currency_count &&
            g_converter.to_index[g_mode] >= 0 &&
            g_converter.to_index[g_mode] < g_currency_count) {
            wchar_t one_rate[96];
            double unit_rate = extras_convert_value(
                MODE_CURRENCY, g_converter.from_index[g_mode],
                g_converter.to_index[g_mode], 1.0);
            format_double_wide(unit_rate, one_rate, _countof(one_rate));
            _snwprintf(rate_text, _countof(rate_text), L"1 %ls = %ls %ls",
                       g_currency[g_converter.from_index[g_mode]].code,
                       one_rate,
                       g_currency[g_converter.to_index[g_mode]].code);
            rate_text[_countof(rate_text) - 1] = L'\0';
        }
    } else {
        format_double_wide(input_value, input, _countof(input));
        format_double_wide(converted, output, _countof(output));
    }
    round_color(dc, &from_card, RGB(43, 43, 43), sx(7, dpi));
    round_color(dc, &to_card, RGB(43, 43, 43), sx(7, dpi));
    from_value.left += sx(12, dpi);
    from_value.right -= sx(12, dpi);
    from_value.top += sx(6, dpi);
    from_value.bottom = from_unit.top - sx(3, dpi);
    to_value.left += sx(12, dpi);
    to_value.right -= sx(12, dpi);
    to_value.top += sx(6, dpi);
    to_value.bottom = to_unit.top - sx(3, dpi);
    fit_text(dc, input, from_value, dpi, 27, 10, FW_SEMIBOLD, RGB(246, 246, 246),
             DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    fit_text(dc, (g_mode == MODE_CURRENCY && !g_currency_loaded) ? L"Waiting for rates" : output,
             to_value, dpi, 27, 10, FW_SEMIBOLD,
             g_mode == MODE_CURRENCY && !g_currency_loaded ? RGB(170, 175, 176) : RGB(246, 246, 246),
             DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    if (g_mode == MODE_CURRENCY && rate_text[0]) {
        RECT rate = {sx(10, dpi), sx(181, dpi), width - sx(10, dpi), sx(205, dpi)};
        text_color(dc, rate_text, rate, small, RGB(190, 194, 195),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (hot_id == EXTRA_ID_BASE + 400) round_color(dc, &from_unit, RGB(59, 59, 59), sx(4, dpi));
    if (hot_id == EXTRA_ID_BASE + 401) round_color(dc, &to_unit, RGB(59, 59, 59), sx(4, dpi));
    {
        RECT label = from_unit;
        label.left += sx(7, dpi);
        label.right -= sx(28, dpi);
        text_color(dc, converter_unit_label(g_mode, g_converter.from_index[g_mode],
                                             unit_text, _countof(unit_text)),
                   label, normal, RGB(246, 246, 246),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        draw_chevron(dc, from_unit, dpi, RGB(170, 175, 176));
    }
    {
        RECT label = to_unit;
        label.left += sx(7, dpi);
        label.right -= sx(28, dpi);
        text_color(dc, converter_unit_label(g_mode, g_converter.to_index[g_mode],
                                             unit_text, _countof(unit_text)),
                   label, normal, RGB(246, 246, 246),
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        draw_chevron(dc, to_unit, dpi, RGB(170, 175, 176));
    }
    for (index = 0; index < 16; ++index) {
        int id = EXTRA_ID_BASE + 420 + index;
        RECT rect = table_button_rect(index, 4, 4, sx(327, dpi), width, keypad_height, dpi);
        COLORREF fill = index == 11 ? RGB(73, 85, 90) :
                        ((index <= 2) || (index >= 4 && index <= 6) ||
                         (index >= 8 && index <= 10) ||
                         index == 12 || index == 13 || index == 14)
                            ? RGB(59, 59, 59) : RGB(50, 50, 50);
        if (!keypad[index][0]) fill = RGB(37, 37, 37);
        else if (id == pressed_id) fill = RGB(78, 78, 78);
        else if (id == hot_id) fill = RGB(69, 69, 69);
        round_color(dc, &rect, fill, sx(5, dpi));
        text_color(dc, keypad[index], rect, normal, RGB(246, 246, 246),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (g_mode == MODE_CURRENCY) {
        RECT source = currency_source_rect(width, dpi);
        RECT gathered = {sx(10, dpi), height - sx(16, dpi),
                         width - sx(10, dpi), height - sx(3, dpi)};
        text_color(dc, g_currency_status, source, small, RGB(156, 198, 217),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (hot_id == EXTRA_ID_CURRENCY_SOURCE) {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(156, 198, 217));
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, source.left + sx(48, dpi), source.bottom - sx(1, dpi), NULL);
            LineTo(dc, source.right - sx(48, dpi), source.bottom - sx(1, dpi));
            SelectObject(dc, old);
            DeleteObject(pen);
        }
        text_color(dc, g_currency_gathered[0] ? g_currency_gathered : L"No saved rates yet",
                   gathered, small, RGB(170, 175, 176),
                   DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (g_converter.picker_open)
        draw_converter_picker(dc, width, height, dpi, hot_id, pressed_id);
    DeleteObject(small);
    DeleteObject(normal);
}

enum {
    EXTRA_TEXT_SCIENTIFIC = 1,
    EXTRA_TEXT_PROGRAMMER = 10,
    EXTRA_TEXT_PROGRAMMER_HEX = 20,
    EXTRA_TEXT_PROGRAMMER_DEC,
    EXTRA_TEXT_PROGRAMMER_OCT,
    EXTRA_TEXT_PROGRAMMER_BIN,
    EXTRA_TEXT_DATE_FIRST = 30,
    EXTRA_TEXT_DATE_SECOND,
    EXTRA_TEXT_DATE_RESULT,
    EXTRA_TEXT_CONVERTER_INPUT = 40,
    EXTRA_TEXT_CONVERTER_OUTPUT,
    EXTRA_TEXT_CONVERTER_RATE
};

typedef struct ExtraTextField {
    RECT rect;
    wchar_t text[256];
    int points;
    int minimum_points;
    int weight;
    UINT format;
} ExtraTextField;

static int extra_default_text_field(void) {
    if (g_mode == MODE_SCIENTIFIC) return EXTRA_TEXT_SCIENTIFIC;
    if (g_mode == MODE_PROGRAMMER) return EXTRA_TEXT_PROGRAMMER;
    if (g_mode == MODE_DATE)
        return g_date.add_mode ? EXTRA_TEXT_DATE_RESULT : EXTRA_TEXT_DATE_FIRST;
    if (g_mode >= MODE_CURRENCY) return EXTRA_TEXT_CONVERTER_INPUT;
    return 0;
}

static int extra_get_text_field(int field, int width, int height, UINT dpi,
                                ExtraTextField *output) {
    if (!output) return 0;
    ZeroMemory(output, sizeof(*output));
    output->weight = FW_SEMIBOLD;
    output->format = DT_RIGHT | DT_VCENTER | DT_SINGLELINE;
    if (field == EXTRA_TEXT_SCIENTIFIC && g_mode == MODE_SCIENTIFIC) {
        output->rect.left = sx(10, dpi);
        output->rect.top = sx(78, dpi);
        output->rect.right = width - sx(12, dpi);
        output->rect.bottom = sx(151, dpi);
        if (g_scientific.error)
            copy_wide(output->text, _countof(output->text), L"Invalid input");
        else
            format_double_wide(scientific_value(), output->text,
                               _countof(output->text));
        output->points = 32;
        output->minimum_points = 11;
        return 1;
    }
    if (field == EXTRA_TEXT_PROGRAMMER && g_mode == MODE_PROGRAMMER) {
        output->rect.left = sx(10, dpi);
        output->rect.top = sx(52, dpi);
        output->rect.right = width - sx(10, dpi);
        output->rect.bottom = sx(103, dpi);
        if (g_programmer.error)
            copy_wide(output->text, _countof(output->text), L"Invalid operation");
        else
            format_programmer(g_programmer.value, g_programmer.base,
                              output->text, _countof(output->text));
        output->points = 25;
        output->minimum_points = 10;
        return 1;
    }
    if (field >= EXTRA_TEXT_PROGRAMMER_HEX &&
        field <= EXTRA_TEXT_PROGRAMMER_BIN &&
        g_mode == MODE_PROGRAMMER) {
        static const int bases[] = {16, 10, 8, 2};
        int index = field - EXTRA_TEXT_PROGRAMMER_HEX;
        output->rect = programmer_base_rect(index, width, dpi);
        output->rect.left += sx(48, dpi);
        output->rect.right -= sx(4, dpi);
        if (g_programmer.error)
            copy_wide(output->text, _countof(output->text), L"Invalid operation");
        else
            format_programmer(g_programmer.value, bases[index],
                              output->text, _countof(output->text));
        output->points = 11;
        output->minimum_points = 7;
        output->weight = FW_NORMAL;
        return 1;
    }
    if (g_mode == MODE_DATE &&
        (field == EXTRA_TEXT_DATE_FIRST ||
         field == EXTRA_TEXT_DATE_SECOND ||
         field == EXTRA_TEXT_DATE_RESULT)) {
        SYSTEMTIME result = g_date.first;
        long long days;
        if (field == EXTRA_TEXT_DATE_FIRST) {
            output->rect = date_field_rect(0, width, dpi);
            output->rect.left += sx(10, dpi);
            output->rect.right -= sx(38, dpi);
            format_date_long(&g_date.first, output->text, _countof(output->text));
            output->points = 12;
            output->minimum_points = 8;
            output->weight = FW_NORMAL;
            output->format = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
            return 1;
        }
        if (field == EXTRA_TEXT_DATE_SECOND && !g_date.add_mode) {
            output->rect = date_field_rect(1, width, dpi);
            output->rect.left += sx(10, dpi);
            output->rect.right -= sx(38, dpi);
            format_date_long(&g_date.second, output->text, _countof(output->text));
            output->points = 12;
            output->minimum_points = 8;
            output->weight = FW_NORMAL;
            output->format = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
            return 1;
        }
        if (field == EXTRA_TEXT_DATE_RESULT) {
            output->rect = date_result_rect(width, height, dpi);
            output->format = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
            if (g_date.add_mode) {
                adjust_date(&result, g_date.add_unit,
                            (g_date.subtract ? -1 : 1) * g_date.add_amount);
                format_date(&result, output->text, _countof(output->text));
                output->points = 27;
                output->minimum_points = 13;
            } else {
                days = date_difference_days(&g_date.first, &g_date.second);
                _snwprintf(output->text, _countof(output->text),
                           L"%lld day%ls apart", days, days == 1 ? L"" : L"s");
                output->points = 25;
                output->minimum_points = 12;
            }
            output->text[_countof(output->text) - 1] = L'\0';
            return 1;
        }
    }
    if (g_mode >= MODE_CURRENCY &&
        (field == EXTRA_TEXT_CONVERTER_INPUT ||
         field == EXTRA_TEXT_CONVERTER_OUTPUT ||
         field == EXTRA_TEXT_CONVERTER_RATE)) {
        double input_value = strtod(g_converter.input[g_mode], NULL);
        double converted = extras_convert_value(
            g_mode, g_converter.from_index[g_mode],
            g_converter.to_index[g_mode], input_value);
        RECT card;
        RECT unit;
        if (field == EXTRA_TEXT_CONVERTER_RATE) {
            if (g_mode != MODE_CURRENCY || !g_currency_loaded ||
                g_converter.from_index[g_mode] >= g_currency_count ||
                g_converter.to_index[g_mode] >= g_currency_count)
                return 0;
            output->rect.left = sx(10, dpi);
            output->rect.top = sx(181, dpi);
            output->rect.right = width - sx(10, dpi);
            output->rect.bottom = sx(205, dpi);
            {
                wchar_t rate[128];
                double one = extras_convert_value(
                    MODE_CURRENCY, g_converter.from_index[g_mode],
                    g_converter.to_index[g_mode], 1.0);
                format_double_wide(one, rate, _countof(rate));
                _snwprintf(output->text, _countof(output->text),
                           L"1 %ls = %ls %ls",
                           g_currency[g_converter.from_index[g_mode]].code,
                           rate,
                           g_currency[g_converter.to_index[g_mode]].code);
            }
            output->points = 9;
            output->minimum_points = 7;
            output->weight = FW_NORMAL;
            output->format = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
            return 1;
        }
        card = converter_card_rect(field == EXTRA_TEXT_CONVERTER_OUTPUT,
                                   width, dpi);
        unit = converter_unit_rect(field == EXTRA_TEXT_CONVERTER_OUTPUT,
                                   width, dpi);
        output->rect = card;
        output->rect.left += sx(12, dpi);
        output->rect.right -= sx(12, dpi);
        output->rect.top += sx(6, dpi);
        output->rect.bottom = unit.top - sx(3, dpi);
        if (g_mode == MODE_CURRENCY) {
            if (!g_currency_loaded && field == EXTRA_TEXT_CONVERTER_OUTPUT)
                copy_wide(output->text, _countof(output->text), L"Waiting for rates");
            else
                format_currency_amount(
                    field == EXTRA_TEXT_CONVERTER_INPUT ? input_value : converted,
                    field == EXTRA_TEXT_CONVERTER_INPUT
                        ? g_converter.from_index[g_mode]
                        : g_converter.to_index[g_mode],
                    output->text, _countof(output->text));
        } else {
            format_double_wide(
                field == EXTRA_TEXT_CONVERTER_INPUT ? input_value : converted,
                output->text, _countof(output->text));
        }
        output->points = 27;
        output->minimum_points = 10;
        return 1;
    }
    return 0;
}

static HFONT extra_fitted_field_font(HDC dc, const ExtraTextField *field,
                                     UINT dpi) {
    HFONT font = NULL;
    HGDIOBJ old;
    SIZE size;
    int points = field->points;
    int available = field->rect.right - field->rect.left;
    do {
        if (font) DeleteObject(font);
        font = extra_font(dpi, points, field->weight);
        old = SelectObject(dc, font);
        GetTextExtentPoint32W(dc, field->text, (int)wcslen(field->text), &size);
        SelectObject(dc, old);
        if (size.cx <= available || points <= field->minimum_points) break;
        --points;
    } while (points >= field->minimum_points);
    return font;
}

static int extra_text_character_from_x(HWND owner, int field_id, int mouse_x,
                                       int width, int height, UINT dpi) {
    ExtraTextField field;
    HDC dc;
    HFONT font;
    HGDIOBJ old;
    SIZE full = {0, 0};
    SIZE prefix = {0, 0};
    int text_x;
    int previous = 0;
    int index;
    int length;
    if (!extra_get_text_field(field_id, width, height, dpi, &field)) return 0;
    dc = GetDC(owner);
    if (!dc) return 0;
    font = extra_fitted_field_font(dc, &field, dpi);
    old = SelectObject(dc, font);
    length = (int)wcslen(field.text);
    GetTextExtentPoint32W(dc, field.text, length, &full);
    if (field.format & DT_RIGHT) text_x = field.rect.right - full.cx;
    else if (field.format & DT_CENTER)
        text_x = field.rect.left + (field.rect.right - field.rect.left - full.cx) / 2;
    else text_x = field.rect.left;
    if (mouse_x <= text_x) index = 0;
    else if (mouse_x >= text_x + full.cx) index = length;
    else {
        index = length;
        for (int current = 0; current < length; ++current) {
            GetTextExtentPoint32W(dc, field.text, current + 1, &prefix);
            if (mouse_x < text_x + (previous + prefix.cx) / 2) {
                index = current;
                break;
            }
            previous = prefix.cx;
        }
    }
    SelectObject(dc, old);
    DeleteObject(font);
    ReleaseDC(owner, dc);
    return index;
}

static void draw_extra_text_selection(HDC dc, int width, int height, UINT dpi) {
    ExtraTextField field;
    HFONT font;
    HGDIOBJ old;
    TEXTMETRICW metrics;
    SIZE full = {0, 0};
    SIZE start_size = {0, 0};
    SIZE end_size = {0, 0};
    RECT highlight;
    int start = g_text_selection.anchor;
    int end = g_text_selection.end;
    int length;
    int text_x;
    if (!g_text_selection.field ||
        !extra_get_text_field(g_text_selection.field, width, height, dpi, &field))
        return;
    length = (int)wcslen(field.text);
    if (start > length) start = length;
    if (end > length) end = length;
    if (start == end) return;
    if (start > end) {
        int swap = start;
        start = end;
        end = swap;
    }
    font = extra_fitted_field_font(dc, &field, dpi);
    old = SelectObject(dc, font);
    GetTextMetricsW(dc, &metrics);
    GetTextExtentPoint32W(dc, field.text, length, &full);
    GetTextExtentPoint32W(dc, field.text, start, &start_size);
    GetTextExtentPoint32W(dc, field.text, end, &end_size);
    if (field.format & DT_RIGHT) text_x = field.rect.right - full.cx;
    else if (field.format & DT_CENTER)
        text_x = field.rect.left + (field.rect.right - field.rect.left - full.cx) / 2;
    else text_x = field.rect.left;
    highlight.left = text_x + start_size.cx;
    highlight.right = text_x + end_size.cx;
    highlight.top = (field.rect.top + field.rect.bottom - metrics.tmHeight) / 2;
    highlight.bottom = highlight.top + metrics.tmHeight;
    if (highlight.left < field.rect.left) highlight.left = field.rect.left;
    if (highlight.right > field.rect.right) highlight.right = field.rect.right;
    if (highlight.right > highlight.left)
        fill_color(dc, &highlight, RGB(82, 82, 82));
    SelectObject(dc, old);
    text_color(dc, field.text, field.rect, font, RGB(246, 246, 246), field.format);
    DeleteObject(font);
}

int extras_text_hit_test(int x, int y, int width, int height, UINT dpi) {
    static const int fields[] = {
        EXTRA_TEXT_SCIENTIFIC,
        EXTRA_TEXT_PROGRAMMER,
        EXTRA_TEXT_PROGRAMMER_HEX,
        EXTRA_TEXT_PROGRAMMER_DEC,
        EXTRA_TEXT_PROGRAMMER_OCT,
        EXTRA_TEXT_PROGRAMMER_BIN,
        EXTRA_TEXT_DATE_FIRST,
        EXTRA_TEXT_DATE_SECOND,
        EXTRA_TEXT_DATE_RESULT,
        EXTRA_TEXT_CONVERTER_INPUT,
        EXTRA_TEXT_CONVERTER_OUTPUT,
        EXTRA_TEXT_CONVERTER_RATE
    };
    ExtraTextField field;
    size_t index;
    if (g_converter.picker_open || g_date.calendar_target) return 0;
    for (index = 0; index < _countof(fields); ++index) {
        if (extra_get_text_field(fields[index], width, height, dpi, &field) &&
            inside(field.rect, x, y))
            return fields[index];
    }
    return 0;
}

void extras_begin_text_selection(HWND owner, int x, int y,
                                 int width, int height, UINT dpi) {
    int field = extras_text_hit_test(x, y, width, height, dpi);
    if (!field) return;
    g_text_selection.field = field;
    g_text_selection.anchor = extra_text_character_from_x(
        owner, field, x, width, height, dpi);
    g_text_selection.end = g_text_selection.anchor;
    g_text_selection.dragging = 1;
    InvalidateRect(owner, NULL, FALSE);
}

void extras_update_text_selection(HWND owner, int x,
                                  int width, int height, UINT dpi) {
    int next;
    if (!g_text_selection.dragging || !g_text_selection.field) return;
    next = extra_text_character_from_x(owner, g_text_selection.field,
                                       x, width, height, dpi);
    if (next != g_text_selection.end) {
        g_text_selection.end = next;
        InvalidateRect(owner, NULL, FALSE);
    }
}

void extras_end_text_selection(HWND owner) {
    g_text_selection.dragging = 0;
    InvalidateRect(owner, NULL, FALSE);
}

int extras_text_selection_dragging(void) {
    return g_text_selection.dragging;
}

void extras_focus_text_at(HWND owner, int x, int y,
                          int width, int height, UINT dpi) {
    int field = extras_text_hit_test(x, y, width, height, dpi);
    if (!field) return;
    if (g_text_selection.field != field) {
        g_text_selection.field = field;
        g_text_selection.anchor = 0;
        g_text_selection.end = 0;
    }
    g_text_selection.dragging = 0;
    InvalidateRect(owner, NULL, FALSE);
}

void extras_clear_text_selection(void) {
    ZeroMemory(&g_text_selection, sizeof(g_text_selection));
}

static int set_extra_clipboard_text(HWND owner, const wchar_t *text) {
    HGLOBAL memory;
    wchar_t *target;
    if (!text || !OpenClipboard(owner)) return 0;
    EmptyClipboard();
    memory = GlobalAlloc(GMEM_MOVEABLE, (wcslen(text) + 1) * sizeof(wchar_t));
    if (!memory) {
        CloseClipboard();
        return 0;
    }
    target = (wchar_t *)GlobalLock(memory);
    if (!target) {
        GlobalFree(memory);
        CloseClipboard();
        return 0;
    }
    wcscpy(target, text);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        return 0;
    }
    CloseClipboard();
    return 1;
}

int extras_copy_text(HWND owner, int width, int height, UINT dpi) {
    ExtraTextField field;
    wchar_t selected[256];
    int field_id = g_text_selection.field
                       ? g_text_selection.field : extra_default_text_field();
    int start = g_text_selection.anchor;
    int end = g_text_selection.end;
    int length;
    if (!extra_get_text_field(field_id, width, height, dpi, &field)) return 0;
    length = (int)wcslen(field.text);
    if (start > length) start = length;
    if (end > length) end = length;
    if (field_id == g_text_selection.field && start != end) {
        int count;
        if (start > end) {
            int swap = start;
            start = end;
            end = swap;
        }
        count = end - start;
        if (count >= (int)_countof(selected)) count = (int)_countof(selected) - 1;
        wcsncpy(selected, field.text + start, (size_t)count);
        selected[count] = L'\0';
        return set_extra_clipboard_text(owner, selected);
    }
    return set_extra_clipboard_text(owner, field.text);
}

static int clipboard_decimal_value(HWND owner, double *value) {
    HANDLE data;
    const wchar_t *source;
    char normalized[160];
    size_t used = 0;
    int decimal_seen = 0;
    int exponent_seen = 0;
    char *end;
    (void)owner;
    if (!value || !OpenClipboard(owner)) return 0;
    data = GetClipboardData(CF_UNICODETEXT);
    if (!data) {
        CloseClipboard();
        return 0;
    }
    source = (const wchar_t *)GlobalLock(data);
    if (!source) {
        CloseClipboard();
        return 0;
    }
    while (*source && used + 1 < sizeof(normalized)) {
        wchar_t character = *source++;
        if (character >= L'0' && character <= L'9') {
            normalized[used++] = (char)character;
        } else if ((character == L'-' || character == L'−' || character == L'+') &&
                   (used == 0 || (used > 0 && normalized[used - 1] == 'e'))) {
            normalized[used++] = character == L'+' ? '+' : '-';
        } else if ((character == L'.' || character == L',') &&
                   !decimal_seen && !exponent_seen) {
            normalized[used++] = '.';
            decimal_seen = 1;
        } else if ((character == L'e' || character == L'E') &&
                   used > 0 && !exponent_seen) {
            normalized[used++] = 'e';
            exponent_seen = 1;
        }
    }
    normalized[used] = '\0';
    GlobalUnlock(data);
    CloseClipboard();
    if (!used) return 0;
    *value = strtod(normalized, &end);
    return end != normalized && *end == '\0' && isfinite(*value);
}

static int clipboard_programmer_value(HWND owner, int base, uint64_t *value) {
    HANDLE data;
    const wchar_t *source;
    char normalized[96];
    size_t used = 0;
    char *end;
    if (!value || !OpenClipboard(owner)) return 0;
    data = GetClipboardData(CF_UNICODETEXT);
    if (!data) {
        CloseClipboard();
        return 0;
    }
    source = (const wchar_t *)GlobalLock(data);
    if (!source) {
        CloseClipboard();
        return 0;
    }
    while (*source && used + 1 < sizeof(normalized)) {
        wchar_t character = towupper(*source++);
        int digit = character >= L'0' && character <= L'9'
                        ? (int)(character - L'0')
                        : character >= L'A' && character <= L'F'
                              ? 10 + (int)(character - L'A') : -1;
        if (digit >= 0 && digit < base) normalized[used++] = (char)character;
    }
    normalized[used] = '\0';
    GlobalUnlock(data);
    CloseClipboard();
    if (!used) return 0;
    *value = strtoull(normalized, &end, base);
    return end != normalized && *end == '\0';
}

static int clipboard_date_value(HWND owner, SYSTEMTIME *date) {
    HANDLE data;
    const wchar_t *source;
    unsigned first;
    unsigned second;
    unsigned third;
    wchar_t month_name[64];
    SYSTEMTIME candidate;
    FILETIME checked;
    int parsed = 0;
    if (!date || !OpenClipboard(owner)) return 0;
    data = GetClipboardData(CF_UNICODETEXT);
    if (!data) {
        CloseClipboard();
        return 0;
    }
    source = (const wchar_t *)GlobalLock(data);
    if (source) {
        ZeroMemory(&candidate, sizeof(candidate));
        if (swscanf(source, L"%u-%u-%u", &first, &second, &third) == 3 &&
            first >= 1601) {
            candidate.wYear = (WORD)first;
            candidate.wMonth = (WORD)second;
            candidate.wDay = (WORD)third;
            parsed = 1;
        } else if (swscanf(source, L"%u/%u/%u", &first, &second, &third) == 3) {
            candidate.wDay = (WORD)first;
            candidate.wMonth = (WORD)second;
            candidate.wYear = (WORD)third;
            parsed = 1;
        } else if (swscanf(source, L"%u %63ls %u",
                           &first, month_name, &third) == 3) {
            int month;
            for (month = 1; month <= 12; ++month) {
                SYSTEMTIME month_date;
                wchar_t expected[64];
                ZeroMemory(&month_date, sizeof(month_date));
                month_date.wYear = 2024;
                month_date.wMonth = (WORD)month;
                month_date.wDay = 1;
                if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &month_date,
                                    L"MMMM", expected, (int)_countof(expected), NULL) &&
                    _wcsicmp(month_name, expected) == 0) {
                    candidate.wDay = (WORD)first;
                    candidate.wMonth = (WORD)month;
                    candidate.wYear = (WORD)third;
                    parsed = 1;
                    break;
                }
            }
        }
        GlobalUnlock(data);
    }
    CloseClipboard();
    if (!parsed || !SystemTimeToFileTime(&candidate, &checked)) return 0;
    *date = candidate;
    return 1;
}

int extras_paste_text(HWND owner) {
    double decimal;
    if (g_mode == MODE_SCIENTIFIC) {
        if (!clipboard_decimal_value(owner, &decimal)) return 0;
        scientific_set(decimal);
    } else if (g_mode == MODE_PROGRAMMER) {
        uint64_t value;
        int base = g_programmer.base;
        if (g_text_selection.field >= EXTRA_TEXT_PROGRAMMER_HEX &&
            g_text_selection.field <= EXTRA_TEXT_PROGRAMMER_BIN) {
            static const int bases[] = {16, 10, 8, 2};
            base = bases[g_text_selection.field - EXTRA_TEXT_PROGRAMMER_HEX];
        }
        if (!clipboard_programmer_value(owner, base, &value)) return 0;
        g_programmer.value = value & programmer_mask();
        g_programmer.new_input = 0;
        g_programmer.error = 0;
    } else if (g_mode == MODE_DATE) {
        SYSTEMTIME date;
        if (!clipboard_date_value(owner, &date)) return 0;
        if (g_text_selection.field == EXTRA_TEXT_DATE_SECOND && !g_date.add_mode)
            g_date.second = date;
        else
            g_date.first = date;
    } else if (g_mode >= MODE_CURRENCY) {
        if (!clipboard_decimal_value(owner, &decimal)) return 0;
        format_double_ascii(decimal, g_converter.input[g_mode],
                            sizeof(g_converter.input[g_mode]));
        g_converter.new_input[g_mode] = 0;
    } else {
        return 0;
    }
    extras_clear_text_selection();
    InvalidateRect(owner, NULL, FALSE);
    return 1;
}

void extras_paint(HDC dc, int width, int height, UINT dpi, int hot_id, int pressed_id) {
    RECT panel = {0, sx(50, dpi), width, height};
    fill_color(dc, &panel, RGB(37, 37, 37));
    if (g_mode == MODE_SCIENTIFIC) draw_scientific(dc, width, height, dpi, hot_id, pressed_id);
    else if (g_mode == MODE_PROGRAMMER) draw_programmer(dc, width, height, dpi, hot_id, pressed_id);
    else if (g_mode == MODE_DATE) draw_date(dc, width, height, dpi, hot_id, pressed_id);
    else if (g_mode >= MODE_CURRENCY)
        draw_converter(dc, width, height, dpi, hot_id, pressed_id);
    draw_extra_text_selection(dc, width, height, dpi);
}

int extras_hit_test(int x, int y, int width, int height, UINT dpi) {
    int index;
    if (g_mode == MODE_SCIENTIFIC) {
        if (g_scientific.popup) {
            int count = g_scientific.popup == 1 ? 8 : 6;
            int columns = g_scientific.popup == 1 ? 4 : 3;
            int base = g_scientific.popup == 1 ? 70 : 80;
            for (index = 0; index < count; ++index)
                if (inside(scientific_popup_item_rect(index, columns, width, dpi),
                           x, y))
                    return EXTRA_ID_BASE + base + index;
        }
        {
            RECT angle = {sx(12, dpi), sx(151, dpi), sx(75, dpi), sx(181, dpi)};
            RECT fe = {sx(78, dpi), sx(151, dpi), sx(132, dpi), sx(181, dpi)};
            if (inside(angle, x, y)) return EXTRA_ID_BASE + 60;
            if (inside(fe, x, y)) return EXTRA_ID_BASE + 61;
        }
        for (index = 0; index < 2; ++index)
            if (inside(scientific_option_rect(index, width, dpi), x, y))
                return EXTRA_ID_BASE + 62 + index;
        for (index = 0; index < 35; ++index)
            if (inside(table_button_rect(index, 5, 7, sx(225, dpi),
                                         width, height, dpi), x, y))
                return EXTRA_ID_BASE + index;
    } else if (g_mode == MODE_PROGRAMMER) {
        if (g_programmer.popup) {
            int count = g_programmer.popup == 1 ? 6 : 4;
            int base = g_programmer.popup == 1 ? 170 : 180;
            for (index = 0; index < count; ++index)
                if (inside(programmer_popup_item_rect(index, width, dpi), x, y))
                    return EXTRA_ID_BASE + base + index;
        }
        for (index = 0; index < 4; ++index)
            if (inside(programmer_base_rect(index, width, dpi), x, y))
                return EXTRA_ID_BASE + 100 + index;
        for (index = 0; index < 4; ++index)
            if (inside(programmer_tool_rect(index, width, dpi), x, y))
                return EXTRA_ID_BASE + 160 + index;
        if (g_programmer.bit_keypad) {
            for (index = 0; index < g_programmer.bits; ++index)
                if (inside(programmer_bit_rect(index, width, dpi), x, y))
                    return EXTRA_ID_BASE + 700 + index;
        } else {
            for (index = 0; index < 30; ++index) {
                int digit = programmer_digit_for_button(index);
                int disabled = (digit >= g_programmer.base && digit >= 0) ||
                               index == 28;
                if (disabled) continue;
                if (inside(table_button_rect(index, 5, 6, sx(237, dpi),
                                             width, height, dpi), x, y))
                    return EXTRA_ID_BASE + 120 + index;
            }
        }
    } else if (g_mode == MODE_DATE) {
        if (g_date.calendar_target) {
            if (inside(date_calendar_arrow_rect(0, width, height, dpi), x, y))
                return EXTRA_ID_BASE + 220;
            if (inside(date_calendar_arrow_rect(1, width, height, dpi), x, y))
                return EXTRA_ID_BASE + 221;
            for (index = 0; index < 42; ++index)
                if (inside(date_calendar_cell_rect(index, width, height, dpi), x, y))
                    return EXTRA_ID_BASE + 222 + index;
            return -1;
        }
        for (index = 0; index < 2; ++index)
            if (inside(date_tab_rect(index, width, dpi), x, y))
                return EXTRA_ID_BASE + 200 + index;
        if (inside(date_field_rect(0, width, dpi), x, y))
            return EXTRA_ID_BASE + 210;
        if (!g_date.add_mode &&
            inside(date_field_rect(1, width, dpi), x, y))
            return EXTRA_ID_BASE + 211;
        if (g_date.add_mode) {
            for (index = 0; index < 6; ++index)
                if (inside(table_button_rect(index, 3, 2, sx(261, dpi),
                                             width, sx(347, dpi), dpi), x, y))
                    return EXTRA_ID_BASE + 240 + index;
        }
    } else if (g_mode >= MODE_CURRENCY) {
        if (g_converter.picker_open) {
            int count = extras_unit_count(g_mode);
            int visible = count < EXTRA_MAX_PICKER_ROWS ? count : EXTRA_MAX_PICKER_ROWS;
            int row_height = sx(45, dpi);
            RECT panel = {sx(9, dpi), sx(76, dpi), width - sx(9, dpi), height - sx(8, dpi)};
            for (index = 0; index < visible; ++index) {
                RECT row = {panel.left + sx(5, dpi),
                            panel.top + sx(48, dpi) + index * row_height,
                            panel.right - sx(5, dpi),
                            panel.top + sx(48, dpi) + (index + 1) * row_height - sx(2, dpi)};
                if (inside(row, x, y)) return EXTRA_ID_BASE + 500 + index;
            }
            return -1;
        }
        if (g_mode == MODE_CURRENCY &&
            inside(currency_source_rect(width, dpi), x, y))
            return EXTRA_ID_CURRENCY_SOURCE;
        if (inside(converter_unit_rect(0, width, dpi), x, y)) return EXTRA_ID_BASE + 400;
        if (inside(converter_unit_rect(1, width, dpi), x, y)) return EXTRA_ID_BASE + 401;
        for (index = 0; index < 15; ++index) {
            if (index == 12) continue;
            if (inside(table_button_rect(index, 4, 4, sx(327, dpi),
                                         width,
                                         g_mode == MODE_CURRENCY
                                             ? height - sx(18, dpi) : height,
                                         dpi), x, y))
                return EXTRA_ID_BASE + 420 + index;
        }
    }
    return -1;
}

static void activate_scientific(int index) {
    g_scientific.popup = 0;
    if (index == 0) g_scientific.inverse = !g_scientific.inverse;
    else if (index == 1) scientific_set(3.14159265358979323846);
    else if (index == 2) scientific_set(2.71828182845904523536);
    else if (index == 3) scientific_reset();
    else if (index == 4) scientific_backspace();
    else if (index == 5) scientific_unary(6);
    else if (index == 6) scientific_unary(11);
    else if (index == 7) scientific_unary(10);
    else if (index == 8) scientific_unary(9);
    else if (index == 9) scientific_operator('%');
    else if (index == 10) scientific_unary(5);
    else if (index == 11) scientific_open_group();
    else if (index == 12) scientific_close_group();
    else if (index == 13) scientific_unary(12);
    else if (index == 14) scientific_operator('/');
    else if (index == 15) scientific_operator('^');
    else if (index >= 16 && index <= 18) scientific_digit(7 + index - 16);
    else if (index == 19) scientific_operator('*');
    else if (index == 20) scientific_unary(8);
    else if (index >= 21 && index <= 23) scientific_digit(4 + index - 21);
    else if (index == 24) scientific_operator('-');
    else if (index == 25) scientific_unary(3);
    else if (index >= 26 && index <= 28) scientific_digit(1 + index - 26);
    else if (index == 29) scientific_operator('+');
    else if (index == 30) scientific_unary(4);
    else if (index == 31) scientific_set(-scientific_value());
    else if (index == 32) scientific_digit(0);
    else if (index == 33) scientific_decimal();
    else if (index == 34) {
        if (g_scientific.group_active) scientific_close_group();
        scientific_equals();
    }
}

static void activate_scientific_control(int id) {
    if (id == EXTRA_ID_BASE + 60) {
        g_scientific.angle_mode = (g_scientific.angle_mode + 1) % 3;
    } else if (id == EXTRA_ID_BASE + 61) {
        double current = scientific_value();
        g_scientific.f_e = !g_scientific.f_e;
        scientific_set(current);
    } else if (id == EXTRA_ID_BASE + 62 || id == EXTRA_ID_BASE + 63) {
        int popup = id - (EXTRA_ID_BASE + 61);
        g_scientific.popup = g_scientific.popup == popup ? 0 : popup;
    } else if (id >= EXTRA_ID_BASE + 70 && id < EXTRA_ID_BASE + 78) {
        int item = id - (EXTRA_ID_BASE + 70);
        if (item == 0) g_scientific.inverse = !g_scientific.inverse;
        else if (item >= 1 && item <= 3) scientific_unary(item - 1);
        else if (item == 4) g_scientific.hyperbolic = !g_scientific.hyperbolic;
        else if (item >= 5 && item <= 7) scientific_unary(item + 8);
        g_scientific.popup = 0;
    } else if (id >= EXTRA_ID_BASE + 80 && id < EXTRA_ID_BASE + 86) {
        static const int operations[] = {10, 16, 17, 20, 18, 19};
        scientific_unary(operations[id - (EXTRA_ID_BASE + 80)]);
        g_scientific.popup = 0;
    }
}

static void activate_programmer(int index) {
    int digit = programmer_digit_for_button(index);
    g_programmer.popup = 0;
    if (digit >= 0) programmer_digit(digit);
    else if (index == 1) programmer_operator('<');
    else if (index == 2) programmer_operator('>');
    else if (index == 3) {
        int base = g_programmer.base;
        int bits = g_programmer.bits;
        int shift_mode = g_programmer.shift_mode;
        int bit_keypad = g_programmer.bit_keypad;
        programmer_reset();
        g_programmer.base = base;
        g_programmer.bits = bits;
        g_programmer.shift_mode = shift_mode;
        g_programmer.bit_keypad = bit_keypad;
    } else if (index == 4) {
        g_programmer.value /= (uint64_t)g_programmer.base;
    } else if (index == 6) programmer_open_group();
    else if (index == 7) programmer_close_group();
    else if (index == 8) programmer_operator('%');
    else if (index == 9) programmer_operator('/');
    else if (index == 14) programmer_operator('*');
    else if (index == 19) programmer_operator('-');
    else if (index == 24) programmer_operator('+');
    else if (index == 26) {
        g_programmer.value = (uint64_t)(-(int64_t)g_programmer.value) & programmer_mask();
    } else if (index == 29) {
        if (g_programmer.group_active) programmer_close_group();
        programmer_equals();
    }
}

static void activate_programmer_control(int id) {
    if (id >= EXTRA_ID_BASE + 100 && id < EXTRA_ID_BASE + 104) {
        static const int bases[] = {16, 10, 8, 2};
        g_programmer.base = bases[id - (EXTRA_ID_BASE + 100)];
        g_programmer.popup = 0;
    } else if (id == EXTRA_ID_BASE + 160) {
        g_programmer.bit_keypad = !g_programmer.bit_keypad;
        g_programmer.popup = 0;
    } else if (id == EXTRA_ID_BASE + 161 || id == EXTRA_ID_BASE + 162) {
        int popup = id - (EXTRA_ID_BASE + 160);
        g_programmer.bit_keypad = 0;
        g_programmer.popup = g_programmer.popup == popup ? 0 : popup;
    } else if (id == EXTRA_ID_BASE + 163) {
        if (g_programmer.bits == 64) g_programmer.bits = 32;
        else if (g_programmer.bits == 32) g_programmer.bits = 16;
        else if (g_programmer.bits == 16) g_programmer.bits = 8;
        else g_programmer.bits = 64;
        g_programmer.value &= programmer_mask();
        g_programmer.accumulator &= programmer_mask();
        g_programmer.popup = 0;
    } else if (id >= EXTRA_ID_BASE + 170 && id < EXTRA_ID_BASE + 176) {
        int item = id - (EXTRA_ID_BASE + 170);
        static const int operations[] = {'&', '|', 0, 'A', 'O', '^'};
        if (item == 2) {
            g_programmer.value = (~g_programmer.value) & programmer_mask();
            g_programmer.new_input = 1;
        } else {
            programmer_operator(operations[item]);
        }
        g_programmer.popup = 0;
    } else if (id >= EXTRA_ID_BASE + 180 && id < EXTRA_ID_BASE + 184) {
        g_programmer.shift_mode = id - (EXTRA_ID_BASE + 180);
        g_programmer.popup = 0;
    } else if (id >= EXTRA_ID_BASE + 700 &&
               id < EXTRA_ID_BASE + 700 + g_programmer.bits) {
        int bit = id - (EXTRA_ID_BASE + 700);
        g_programmer.value ^= 1ULL << bit;
        g_programmer.value &= programmer_mask();
        g_programmer.new_input = 0;
    }
}

static void activate_date(int id) {
    if (id >= EXTRA_ID_BASE + 200 && id < EXTRA_ID_BASE + 202) {
        g_date.add_mode = id - (EXTRA_ID_BASE + 200);
        g_date.calendar_target = 0;
        return;
    }
    if (g_date.calendar_target) {
        if (id == EXTRA_ID_BASE + 220 || id == EXTRA_ID_BASE + 221) {
            int value = g_date.calendar_year * 12 + g_date.calendar_month - 1 +
                        (id == EXTRA_ID_BASE + 221 ? 1 : -1);
            if (value < 1601 * 12) value = 1601 * 12;
            if (value > 9999 * 12 + 11) value = 9999 * 12 + 11;
            g_date.calendar_year = value / 12;
            g_date.calendar_month = value % 12 + 1;
        } else if (id >= EXTRA_ID_BASE + 222 &&
                   id < EXTRA_ID_BASE + 222 + 42) {
            SYSTEMTIME selected = date_calendar_cell_value(
                id - (EXTRA_ID_BASE + 222));
            if (g_date.calendar_target == 2)
                g_date.second = selected;
            else
                g_date.first = selected;
            g_date.calendar_target = 0;
        }
        return;
    }
    if (id == EXTRA_ID_BASE + 210 || id == EXTRA_ID_BASE + 211) {
        const SYSTEMTIME *selected;
        g_date.calendar_target = id == EXTRA_ID_BASE + 211 ? 2 : 1;
        selected = g_date.calendar_target == 2 ? &g_date.second : &g_date.first;
        g_date.calendar_year = selected->wYear;
        g_date.calendar_month = selected->wMonth;
    } else if (id == EXTRA_ID_BASE + 240) {
        if (g_date.add_amount > 1) --g_date.add_amount;
    } else if (id == EXTRA_ID_BASE + 241) {
        if (g_date.add_amount < 1000000) ++g_date.add_amount;
    } else if (id == EXTRA_ID_BASE + 242) {
        g_date.subtract = !g_date.subtract;
    } else if (id >= EXTRA_ID_BASE + 243 && id <= EXTRA_ID_BASE + 245) {
        g_date.add_unit = id - (EXTRA_ID_BASE + 243);
    }
}

static void converter_digit(int digit) {
    char *input = g_converter.input[g_mode];
    int *new_input = &g_converter.new_input[g_mode];
    size_t length;
    if (*new_input || strcmp(input, "0") == 0) {
        input[0] = (char)('0' + digit);
        input[1] = '\0';
        *new_input = 0;
        return;
    }
    length = strlen(input);
    if (length < 18) {
        input[length] = (char)('0' + digit);
        input[length + 1] = '\0';
    }
}

static void converter_decimal(void) {
    char *input = g_converter.input[g_mode];
    int *new_input = &g_converter.new_input[g_mode];
    if (*new_input) {
        copy_ascii(input, sizeof(g_converter.input[g_mode]), "0.");
        *new_input = 0;
    } else if (!strchr(input, '.') &&
               strlen(input) + 1 < sizeof(g_converter.input[g_mode])) {
        strcat(input, ".");
    }
}

static void converter_backspace(void) {
    char *input = g_converter.input[g_mode];
    int *new_input = &g_converter.new_input[g_mode];
    size_t length;
    if (*new_input) return;
    length = strlen(input);
    if (length) input[length - 1] = '\0';
    if (!input[0] || strcmp(input, "-") == 0) {
        copy_ascii(input, sizeof(g_converter.input[g_mode]), "0");
        *new_input = 1;
    }
}

static void activate_converter(int id) {
    int index;
    int selected;
    if (id == EXTRA_ID_CURRENCY_SOURCE) {
        ShellExecuteW(NULL, L"open", L"https://frankfurter.dev/currencies/",
                      NULL, NULL, SW_SHOWNORMAL);
        return;
    }
    if (id == EXTRA_ID_BASE + 400 || id == EXTRA_ID_BASE + 401) {
        g_converter.picker_open = 1;
        g_converter.picker_target = id == EXTRA_ID_BASE + 401;
        g_converter.picker_search[0] = L'\0';
        g_converter.picker_search_tick = 0;
        selected = g_converter.picker_target
                       ? g_converter.to_index[g_mode]
                       : g_converter.from_index[g_mode];
        g_converter.picker_scroll = selected > 3 ? selected - 3 : 0;
        return;
    }
    if (id >= EXTRA_ID_BASE + 500 && id < EXTRA_ID_BASE + 500 + EXTRA_MAX_PICKER_ROWS) {
        index = g_converter.picker_scroll + id - (EXTRA_ID_BASE + 500);
        if (index < extras_unit_count(g_mode)) {
            if (g_converter.picker_target) g_converter.to_index[g_mode] = index;
            else g_converter.from_index[g_mode] = index;
        }
        g_converter.picker_open = 0;
        g_converter.picker_search[0] = L'\0';
        g_converter.picker_search_tick = 0;
        return;
    }
    if (id < EXTRA_ID_BASE + 420 || id >= EXTRA_ID_BASE + 436) return;
    index = id - (EXTRA_ID_BASE + 420);
    if (index <= 2) converter_digit(7 + index);
    else if (index == 3) converter_backspace();
    else if (index >= 4 && index <= 6) converter_digit(4 + index - 4);
    else if (index == 7) {
        copy_ascii(g_converter.input[g_mode], sizeof(g_converter.input[g_mode]), "0");
        g_converter.new_input[g_mode] = 1;
    } else if (index >= 8 && index <= 10) converter_digit(1 + index - 8);
    else if (index == 11) {
        int swap = g_converter.from_index[g_mode];
        g_converter.from_index[g_mode] = g_converter.to_index[g_mode];
        g_converter.to_index[g_mode] = swap;
    } else if (index == 13) converter_digit(0);
    else if (index == 14) converter_decimal();
}

void extras_activate(HWND owner, int id) {
    extras_clear_text_selection();
    if (g_mode == MODE_SCIENTIFIC) {
        if (id >= EXTRA_ID_BASE && id < EXTRA_ID_BASE + 35)
            activate_scientific(id - EXTRA_ID_BASE);
        else
            activate_scientific_control(id);
    }
    else if (g_mode == MODE_PROGRAMMER) {
        if (id >= EXTRA_ID_BASE + 120 && id < EXTRA_ID_BASE + 150)
            activate_programmer(id - (EXTRA_ID_BASE + 120));
        else
            activate_programmer_control(id);
    } else if (g_mode == MODE_DATE) activate_date(id);
    else if (g_mode >= MODE_CURRENCY) activate_converter(id);
    InvalidateRect(owner, NULL, FALSE);
}

void extras_mouse_wheel(HWND owner, int wheel_steps) {
    int count;
    int maximum;
    if (g_mode < MODE_CURRENCY || !g_converter.picker_open) return;
    count = extras_unit_count(g_mode);
    maximum = count > EXTRA_MAX_PICKER_ROWS ? count - EXTRA_MAX_PICKER_ROWS : 0;
    g_converter.picker_scroll -= wheel_steps * 2;
    g_converter.picker_search[0] = L'\0';
    g_converter.picker_search_tick = 0;
    if (g_converter.picker_scroll < 0) g_converter.picker_scroll = 0;
    if (g_converter.picker_scroll > maximum) g_converter.picker_scroll = maximum;
    InvalidateRect(owner, NULL, FALSE);
}

void extras_key_down(HWND owner, WPARAM key) {
    if (g_converter.picker_open &&
        (key == VK_UP || key == VK_DOWN || key == VK_HOME ||
         key == VK_END || key == VK_RETURN)) {
        int count = extras_unit_count(g_mode);
        int maximum = count > EXTRA_MAX_PICKER_ROWS
                          ? count - EXTRA_MAX_PICKER_ROWS : 0;
        if (key == VK_UP && g_converter.picker_scroll > 0)
            --g_converter.picker_scroll;
        else if (key == VK_DOWN && g_converter.picker_scroll < maximum)
            ++g_converter.picker_scroll;
        else if (key == VK_HOME)
            g_converter.picker_scroll = 0;
        else if (key == VK_END)
            g_converter.picker_scroll = maximum;
        else if (key == VK_RETURN && count > 0) {
            int selected = g_converter.picker_scroll;
            if (g_converter.picker_target)
                g_converter.to_index[g_mode] = selected;
            else
                g_converter.from_index[g_mode] = selected;
            g_converter.picker_open = 0;
        }
        if (key != VK_RETURN) {
            g_converter.picker_search[0] = L'\0';
            g_converter.picker_search_tick = 0;
        }
        InvalidateRect(owner, NULL, FALSE);
        return;
    }
    if (key != VK_CONTROL && key != VK_SHIFT)
        extras_clear_text_selection();
    if (key == VK_ESCAPE) {
        if (g_converter.picker_open) {
            g_converter.picker_open = 0;
            g_converter.picker_search[0] = L'\0';
            g_converter.picker_search_tick = 0;
        }
        else if (g_mode == MODE_DATE && g_date.calendar_target)
            g_date.calendar_target = 0;
        else if (g_mode == MODE_SCIENTIFIC && g_scientific.popup)
            g_scientific.popup = 0;
        else if (g_mode == MODE_PROGRAMMER && g_programmer.popup)
            g_programmer.popup = 0;
        else if (g_mode == MODE_SCIENTIFIC) scientific_reset();
        else if (g_mode == MODE_PROGRAMMER) programmer_reset();
    } else if (key == VK_BACK) {
        if (g_converter.picker_open) {
            size_t length = wcslen(g_converter.picker_search);
            if (length) {
                g_converter.picker_search[length - 1] = L'\0';
                picker_jump_to_search();
            }
            g_converter.picker_search_tick = GetTickCount64();
        } else if (g_mode == MODE_SCIENTIFIC) scientific_backspace();
        else if (g_mode == MODE_PROGRAMMER)
            g_programmer.value /= (uint64_t)g_programmer.base;
        else if (g_mode >= MODE_CURRENCY) converter_backspace();
    } else if (key == VK_DELETE) {
        if (g_mode == MODE_SCIENTIFIC) {
            copy_ascii(g_scientific.display, sizeof(g_scientific.display), "0");
            g_scientific.new_input = 1;
            g_scientific.error = 0;
        } else if (g_mode == MODE_PROGRAMMER) {
            g_programmer.value = 0;
            g_programmer.new_input = 1;
            g_programmer.error = 0;
        } else if (g_mode >= MODE_CURRENCY) {
            copy_ascii(g_converter.input[g_mode], sizeof(g_converter.input[g_mode]), "0");
            g_converter.new_input[g_mode] = 1;
        }
    } else if (key == VK_RETURN) {
        if (g_mode == MODE_SCIENTIFIC) {
            if (g_scientific.group_active) scientific_close_group();
            scientific_equals();
        } else if (g_mode == MODE_PROGRAMMER) {
            if (g_programmer.group_active) programmer_close_group();
            programmer_equals();
        }
    }
    InvalidateRect(owner, NULL, FALSE);
}

void extras_character(HWND owner, wchar_t character) {
    if (g_converter.picker_open) {
        if (iswalnum(character) || character == L' ' || character == L'-') {
            ULONGLONG now = GetTickCount64();
            size_t length;
            if (g_converter.picker_search_tick &&
                now - g_converter.picker_search_tick > 1500)
                g_converter.picker_search[0] = L'\0';
            length = wcslen(g_converter.picker_search);
            if (length + 1 < _countof(g_converter.picker_search)) {
                g_converter.picker_search[length] = character;
                g_converter.picker_search[length + 1] = L'\0';
                picker_jump_to_search();
            }
            g_converter.picker_search_tick = now;
        }
        InvalidateRect(owner, NULL, FALSE);
        return;
    }
    extras_clear_text_selection();
    if (character >= L'0' && character <= L'9') {
        if (g_mode == MODE_SCIENTIFIC) scientific_digit((int)(character - L'0'));
        else if (g_mode == MODE_PROGRAMMER) programmer_digit((int)(character - L'0'));
        else if (g_mode >= MODE_CURRENCY) converter_digit((int)(character - L'0'));
    } else if (g_mode == MODE_PROGRAMMER &&
               ((character >= L'a' && character <= L'f') ||
                (character >= L'A' && character <= L'F'))) {
        programmer_digit(10 + (int)(towupper(character) - L'A'));
    } else if (g_mode == MODE_SCIENTIFIC) {
        if (character == L'.' || character == L',') scientific_decimal();
        else if (character == L'+') scientific_operator('+');
        else if (character == L'-') scientific_operator('-');
        else if (character == L'*' || character == L'x' || character == L'X')
            scientific_operator('*');
        else if (character == L'/') scientific_operator('/');
        else if (character == L'^') scientific_operator('^');
        else if (character == L'=' ) scientific_equals();
    } else if (g_mode == MODE_PROGRAMMER) {
        if (character == L'+') programmer_operator('+');
        else if (character == L'-') programmer_operator('-');
        else if (character == L'*') programmer_operator('*');
        else if (character == L'/') programmer_operator('/');
        else if (character == L'%') programmer_operator('%');
        else if (character == L'&') programmer_operator('&');
        else if (character == L'|') programmer_operator('|');
        else if (character == L'^') programmer_operator('^');
        else if (character == L'=') programmer_equals();
    } else if (g_mode >= MODE_CURRENCY) {
        if (character == L'.' || character == L',') converter_decimal();
    }
    InvalidateRect(owner, NULL, FALSE);
}
