#include "../src/extras.h"

#include <math.h>
#include <stdio.h>

static int failures;

static void expect_close(const char *name, double actual, double expected, double tolerance) {
    if (fabs(actual - expected) > tolerance) {
        fprintf(stderr, "%s: expected %.15g, got %.15g\n", name, expected, actual);
        ++failures;
    }
}

static void expect_true(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "%s: condition failed\n", name);
        ++failures;
    }
}

int main(void) {
    expect_close("kilometres to miles",
                 extras_convert_value(MODE_LENGTH, 5, 9, 5.0),
                 3.1068559611866697, 1e-12);
    expect_close("litres to US gallons",
                 extras_convert_value(MODE_VOLUME, 1, 6, 10.0),
                 2.641720523581484, 1e-12);
    expect_close("celsius to fahrenheit",
                 extras_convert_value(MODE_TEMPERATURE, 0, 1, 20.0),
                 68.0, 1e-12);
    expect_close("fahrenheit to celsius",
                 extras_convert_value(MODE_TEMPERATURE, 1, 0, 32.0),
                 0.0, 1e-12);
    expect_close("kilowatt hours to joules",
                 extras_convert_value(MODE_ENERGY, 5, 0, 1.0),
                 3600000.0, 1e-7);
    expect_close("gibibytes to gigabytes",
                 extras_convert_value(MODE_DATA, 7, 6, 1.0),
                 1.073741824, 1e-12);
    expect_close("degrees to radians",
                 extras_convert_value(MODE_ANGLE, 0, 1, 180.0),
                 3.14159265358979323846, 1e-12);
    expect_true("pressure units", extras_unit_count(MODE_PRESSURE) >= 8);
    expect_true("mode name", extras_mode_name(MODE_DATE)[0] == L'D');
    expect_true("Frankfurter currency metadata",
                extras_currency_metadata_is_complete());
    if (failures) return 1;
    puts("extra-mode tests passed");
    return 0;
}
