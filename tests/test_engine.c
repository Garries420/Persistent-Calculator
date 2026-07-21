#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../src/calc_engine.h"
#include "../src/display_format.h"

static int close_to(double actual, double expected) {
    return fabs(actual - expected) < 1e-10;
}

static void enter_number(CalcState *state, const char *number) {
    while (*number) {
        if (*number == '.') calc_decimal(state);
        else if (*number >= '0' && *number <= '9') calc_digit(state, *number - '0');
        ++number;
    }
}

int main(void) {
    CalcState state;
    char expression[2048];
    char result[96];
    wchar_t formatted[160];

    display_format_ascii_number("500", formatted, 160);
    assert(wcscmp(formatted, L"500") == 0);
    display_format_ascii_number("5000", formatted, 160);
    assert(wcscmp(formatted, L"5 000") == 0);
    display_format_ascii_number("50000", formatted, 160);
    assert(wcscmp(formatted, L"50 000") == 0);
    display_format_ascii_number("500000", formatted, 160);
    assert(wcscmp(formatted, L"500 000") == 0);
    display_format_ascii_number("5000000", formatted, 160);
    assert(wcscmp(formatted, L"5 000 000") == 0);
    display_format_ascii_number("-1234567.89", formatted, 160);
    assert(wcscmp(formatted, L"−1 234 567,89") == 0);
    display_format_ascii_number("2.16163548343314e+24", formatted, 160);
    assert(wcscmp(formatted, L"2,16163548343314e+24") == 0);
    display_format_ascii_number("Cannot divide by zero", formatted, 160);
    assert(wcscmp(formatted, L"Cannot divide by zero") == 0);

    calc_init(&state);
    enter_number(&state, "12.5");
    calc_set_operator(&state, '+');
    enter_number(&state, "7.5");
    assert(calc_equals(&state, expression, sizeof(expression), result, sizeof(result)));
    assert(close_to(calc_value(&state), 20.0));
    assert(strcmp(result, "20") == 0);

    /* Preserve the complete typed chain instead of replacing it with an
       intermediate total after every operator. */
    calc_clear_all(&state);
    enter_number(&state, "20");
    calc_set_operator(&state, '+');
    enter_number(&state, "55");
    calc_set_operator(&state, '+');
    enter_number(&state, "12.5");
    calc_set_operator(&state, '*');
    enter_number(&state, "5");
    assert(strcmp(state.expression, "20 + 55 + 12.5 * 5") == 0);
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(strcmp(expression, "20 + 55 + 12.5 * 5 =") == 0);
    assert(close_to(calc_value(&state), 437.5));

    /* Long chains remain intact well beyond the width of the compact window. */
    calc_clear_all(&state);
    for (int step = 0; step < 100; ++step) {
        enter_number(&state, "1");
        if (step < 99) calc_set_operator(&state, '+');
    }
    assert(strlen(state.expression) > 300);
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(close_to(calc_value(&state), 100.0));
    assert(strlen(expression) > 300);

    /* Recalling a history entry restores its expression and can continue it. */
    calc_restore_history(&state, 437.5, "20 + 55 + 12.5 * 5 =");
    assert(strcmp(state.expression, "20 + 55 + 12.5 * 5 =") == 0);
    assert(close_to(calc_value(&state), 437.5));
    calc_set_operator(&state, '+');
    enter_number(&state, "5");
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(strcmp(expression, "20 + 55 + 12.5 * 5 + 5 =") == 0);
    assert(close_to(calc_value(&state), 442.5));

    calc_restore_history(&state, 437.5, "20 + 55 + 12.5 * 5 =");
    calc_digit(&state, 8);
    assert(close_to(calc_value(&state), 8.0));
    assert(state.expression[0] == '\0');

    /* Equals displays a result without ending the preserved calculation chain. */
    calc_clear_all(&state);
    for (int step = 0; step < 4; ++step) {
        enter_number(&state, "20");
        if (step < 3) calc_set_operator(&state, '+');
    }
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(strcmp(expression, "20 + 20 + 20 + 20 =") == 0);
    assert(close_to(calc_value(&state), 80.0));
    calc_set_operator(&state, '/');
    enter_number(&state, "20");
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(strcmp(expression, "20 + 20 + 20 + 20 = 80 / 20 =") == 0);
    assert(close_to(calc_value(&state), 4.0));

    /* A trailing operator is also an evaluated Standard-mode checkpoint. */
    calc_clear_all(&state);
    enter_number(&state, "11");
    calc_set_operator(&state, '+');
    enter_number(&state, "11");
    calc_set_operator(&state, '+');
    assert(close_to(calc_value(&state), 22.0));
    enter_number(&state, "11");
    calc_set_operator(&state, '+');
    assert(close_to(calc_value(&state), 33.0));
    assert(strcmp(state.expression, "11 + 11 + 11 + ") == 0);

    /* Standard mode evaluates chained operations immediately: (2 + 3) * 4. */
    calc_clear_all(&state);
    enter_number(&state, "2");
    calc_set_operator(&state, '+');
    enter_number(&state, "3");
    calc_set_operator(&state, '*');
    enter_number(&state, "4");
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(close_to(calc_value(&state), 20.0));

    /* Repeated equals repeats the last operation. */
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(close_to(calc_value(&state), 80.0));

    /* 50 + 10% is 55 in the Windows Standard calculator model. */
    calc_clear_all(&state);
    enter_number(&state, "50");
    calc_set_operator(&state, '+');
    enter_number(&state, "10");
    calc_percent(&state);
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(close_to(calc_value(&state), 55.0));

    /* A standalone percentage means "this percent of the next value", as in
       Google Calculator.  The percentage notation is retained for history. */
    calc_clear_all(&state);
    enter_number(&state, "20");
    calc_percent(&state);
    assert(strcmp(state.expression, "20% * ") == 0);
    assert(close_to(calc_value(&state), 20.0));
    enter_number(&state, "100");
    assert(strcmp(state.expression, "20% * 100") == 0);
    assert(calc_equals(&state, expression, sizeof(expression), result, sizeof(result)));
    assert(strcmp(expression, "20% * 100 =") == 0);
    assert(strcmp(result, "20") == 0);
    assert(close_to(calc_value(&state), 20.0));

    /* The next operator also evaluates the percent-of expression, allowing
       the UI's automatic history checkpoint to save it without equals. */
    calc_clear_all(&state);
    enter_number(&state, "25");
    calc_percent(&state);
    enter_number(&state, "80");
    calc_set_operator(&state, '+');
    assert(close_to(calc_value(&state), 20.0));
    assert(strcmp(state.expression, "25% * 80 + ") == 0);

    /* CE clears the current operand rather than repeating the accumulator. */
    calc_clear_all(&state);
    enter_number(&state, "5");
    calc_set_operator(&state, '+');
    enter_number(&state, "8");
    calc_clear_entry(&state);
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(close_to(calc_value(&state), 5.0));

    /* CE clears only the current entry and preserves the earlier chain. */
    calc_clear_all(&state);
    enter_number(&state, "20");
    calc_set_operator(&state, '+');
    enter_number(&state, "20");
    calc_set_operator(&state, '+');
    enter_number(&state, "999");
    calc_clear_entry(&state);
    enter_number(&state, "5");
    calc_equals(&state, expression, sizeof(expression), result, sizeof(result));
    assert(strcmp(expression, "20 + 20 + 5 =") == 0);
    assert(close_to(calc_value(&state), 45.0));

    calc_clear_all(&state);
    enter_number(&state, "9");
    assert(calc_unary(&state, 'q', expression, sizeof(expression), result, sizeof(result)));
    assert(close_to(calc_value(&state), 3.0));

    calc_clear_all(&state);
    enter_number(&state, "10");
    calc_set_operator(&state, '/');
    calc_digit(&state, 0);
    assert(!calc_equals(&state, expression, sizeof(expression), result, sizeof(result)));
    assert(state.error);
    assert(strcmp(state.display, "Cannot divide by zero") == 0);

    calc_clear_all(&state);
    enter_number(&state, "42");
    calc_memory_store(&state);
    calc_clear_entry(&state);
    calc_memory_recall(&state);
    assert(close_to(calc_value(&state), 42.0));

    puts("All calculator engine tests passed.");
    return 0;
}
