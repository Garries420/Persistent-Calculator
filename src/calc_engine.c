#include "calc_engine.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char *destination, size_t size, const char *source) {
    if (!destination || size == 0) return;
    snprintf(destination, size, "%s", source ? source : "");
}

static void format_number(double value, char *output, size_t size) {
    if (!output || size == 0) return;
    if (value == 0.0 || fabs(value) < 1e-300) value = 0.0;
    snprintf(output, size, "%.15g", value);
    if (strcmp(output, "-0") == 0) copy_text(output, size, "0");
}

static void append_text(char *destination, size_t capacity, const char *text) {
    size_t used;
    if (!destination || !text || capacity == 0) return;
    used = strlen(destination);
    if (used + 1 >= capacity) return;
    snprintf(destination + used, capacity - used, "%s", text);
}

static void update_expression_preview(CalcState *state) {
    if (state->chain[0]) {
        copy_text(state->expression, sizeof(state->expression), state->chain);
        if (!state->new_input) append_text(state->expression, sizeof(state->expression), state->display);
    } else if (!state->new_input) {
        /* A single number does not need to be duplicated above the main display. */
        state->expression[0] = '\0';
    }
}

static void replace_chain_operator(CalcState *state, char operation) {
    size_t length = strlen(state->chain);
    while (length > 0 && state->chain[length - 1] == ' ') --length;
    if (length > 0) state->chain[length - 1] = operation;
}

static void set_error(CalcState *state, const char *message) {
    copy_text(state->display, sizeof(state->display), message);
    state->error = 1;
    state->pending_operator = 0;
    state->has_accumulator = 0;
    state->new_input = 1;
}

static int apply_binary(CalcState *state, double left, double right, char operation,
                        double *answer) {
    double result = 0.0;
    switch (operation) {
        case '+': result = left + right; break;
        case '-': result = left - right; break;
        case '*': result = left * right; break;
        case '/':
            if (right == 0.0) {
                set_error(state, left == 0.0 ? "Result is undefined" : "Cannot divide by zero");
                return 0;
            }
            result = left / right;
            break;
        default: return 0;
    }
    if (!isfinite(result)) {
        set_error(state, "Overflow");
        return 0;
    }
    *answer = result;
    return 1;
}

static int digit_count(const char *text) {
    int count = 0;
    while (*text) {
        if (isdigit((unsigned char)*text)) ++count;
        ++text;
    }
    return count;
}

void calc_init(CalcState *state) {
    memset(state, 0, sizeof(*state));
    copy_text(state->display, sizeof(state->display), "0");
    state->new_input = 1;
}

double calc_value(const CalcState *state) {
    if (state->error) return 0.0;
    return strtod(state->display, NULL);
}

void calc_set_value(CalcState *state, double value) {
    if (!isfinite(value)) {
        set_error(state, "Invalid input");
        return;
    }
    format_number(value, state->display, sizeof(state->display));
    state->new_input = state->pending_operator ? 0 : 1;
    state->error = 0;
    state->restored_history = 0;
    state->continuation_available = 0;
    if (state->pending_operator) update_expression_preview(state);
    else {
        state->chain[0] = '\0';
        state->expression[0] = '\0';
    }
}

void calc_restore_history(CalcState *state, double value, const char *expression) {
    size_t length;
    calc_clear_all(state);
    if (!isfinite(value)) {
        set_error(state, "Invalid input");
        return;
    }
    format_number(value, state->display, sizeof(state->display));
    copy_text(state->expression, sizeof(state->expression), expression);
    copy_text(state->chain, sizeof(state->chain), expression);
    length = strlen(state->chain);
    while (length > 0 && state->chain[length - 1] == ' ') --length;
    if (length > 0 && state->chain[length - 1] == '=') --length;
    while (length > 0 && state->chain[length - 1] == ' ') --length;
    state->chain[length] = '\0';
    state->new_input = 1;
    state->restored_history = 1;
    state->continuation_available = 0;
}

void calc_digit(CalcState *state, int digit) {
    size_t length;
    if (digit < 0 || digit > 9) return;
    if (state->error || state->new_input) {
        if (state->error) calc_clear_all(state);
        if (!state->pending_operator) {
            state->chain[0] = '\0';
            state->expression[0] = '\0';
            state->restored_history = 0;
            state->continuation_available = 0;
        }
        copy_text(state->display, sizeof(state->display), "0");
        state->new_input = 0;
        if (!state->pending_operator) state->last_operator = 0;
    }
    if (digit_count(state->display) >= 16) return;
    if (strcmp(state->display, "0") == 0) {
        state->display[0] = (char)('0' + digit);
        state->display[1] = '\0';
        update_expression_preview(state);
        return;
    }
    if (strcmp(state->display, "-0") == 0) {
        state->display[1] = (char)('0' + digit);
        state->display[2] = '\0';
        update_expression_preview(state);
        return;
    }
    length = strlen(state->display);
    if (length + 1 < sizeof(state->display)) {
        state->display[length] = (char)('0' + digit);
        state->display[length + 1] = '\0';
    }
    update_expression_preview(state);
}

void calc_decimal(CalcState *state) {
    if (state->error || state->new_input) {
        if (state->error) calc_clear_all(state);
        if (!state->pending_operator) {
            state->chain[0] = '\0';
            state->expression[0] = '\0';
            state->restored_history = 0;
            state->continuation_available = 0;
        }
        copy_text(state->display, sizeof(state->display), "0.");
        state->new_input = 0;
        if (!state->pending_operator) state->last_operator = 0;
        update_expression_preview(state);
        return;
    }
    if (!strchr(state->display, '.') && !strchr(state->display, 'e') &&
        strlen(state->display) + 1 < sizeof(state->display)) {
        strcat(state->display, ".");
    }
    update_expression_preview(state);
}

void calc_toggle_sign(CalcState *state) {
    size_t length;
    if (state->error || strcmp(state->display, "0") == 0) return;
    if (state->display[0] == '-') {
        memmove(state->display, state->display + 1, strlen(state->display));
    } else {
        length = strlen(state->display);
        if (length + 1 < sizeof(state->display)) {
            memmove(state->display + 1, state->display, length + 1);
            state->display[0] = '-';
        }
    }
    update_expression_preview(state);
}

void calc_backspace(CalcState *state) {
    size_t length;
    if (state->error || state->new_input) return;
    length = strlen(state->display);
    if (length > 0) state->display[length - 1] = '\0';
    if (state->display[0] == '\0' || strcmp(state->display, "-") == 0) {
        copy_text(state->display, sizeof(state->display), "0");
        state->new_input = state->pending_operator ? 0 : 1;
    }
    update_expression_preview(state);
}

void calc_clear_entry(CalcState *state) {
    if (state->error) {
        calc_clear_all(state);
        return;
    }
    copy_text(state->display, sizeof(state->display), "0");
    state->new_input = state->pending_operator ? 0 : 1;
    update_expression_preview(state);
}

void calc_clear_all(CalcState *state) {
    int has_memory = state->has_memory;
    double memory = state->memory;
    calc_init(state);
    state->has_memory = has_memory;
    state->memory = memory;
}

void calc_set_operator(CalcState *state, char operation) {
    double current, answer;
    char current_text[96];
    char operator_text[4] = {' ', 0, ' ', '\0'};
    if (operation != '+' && operation != '-' && operation != '*' && operation != '/') return;
    if (state->error) return;
    current = calc_value(state);
    copy_text(current_text, sizeof(current_text), state->display);
    if (state->pending_operator && state->new_input) {
        state->pending_operator = operation;
        state->last_operator = 0;
        replace_chain_operator(state, operation);
        copy_text(state->expression, sizeof(state->expression), state->chain);
        return;
    }
    if (state->pending_operator && !state->new_input) {
        append_text(state->chain, sizeof(state->chain), current_text);
        if (!apply_binary(state, state->accumulator, current, state->pending_operator, &answer)) return;
        state->accumulator = answer;
        format_number(answer, state->display, sizeof(state->display));
    } else if (!state->has_accumulator) {
        state->accumulator = current;
        state->has_accumulator = 1;
        if ((!state->restored_history && !state->continuation_available) || !state->chain[0]) {
            state->chain[0] = '\0';
            append_text(state->chain, sizeof(state->chain), current_text);
        }
    }
    state->pending_operator = operation;
    state->last_operator = 0;
    state->new_input = 1;
    state->restored_history = 0;
    state->continuation_available = 0;
    operator_text[1] = operation;
    append_text(state->chain, sizeof(state->chain), operator_text);
    copy_text(state->expression, sizeof(state->expression), state->chain);
}

int calc_equals(CalcState *state, char *history_expression, size_t expression_size,
                char *history_result, size_t result_size) {
    double left, right, answer;
    char operation;
    char left_text[96], right_text[96];
    if (state->error) return 0;
    if (state->pending_operator) {
        operation = state->pending_operator;
        left = state->accumulator;
        right = state->new_input ? state->accumulator : calc_value(state);
    } else if (state->last_operator) {
        operation = state->last_operator;
        left = calc_value(state);
        right = state->last_operand;
    } else {
        return 0;
    }
    if (!apply_binary(state, left, right, operation, &answer)) return 0;
    format_number(left, left_text, sizeof(left_text));
    format_number(right, right_text, sizeof(right_text));
    if (state->pending_operator && state->chain[0]) {
        copy_text(history_expression, expression_size, state->chain);
        append_text(history_expression, expression_size, right_text);
        append_text(history_expression, expression_size, " =");
    } else if (state->continuation_available && state->chain[0]) {
        char repeated_operator[4] = {' ', operation, ' ', '\0'};
        copy_text(history_expression, expression_size, state->chain);
        append_text(history_expression, expression_size, repeated_operator);
        append_text(history_expression, expression_size, right_text);
        append_text(history_expression, expression_size, " =");
    } else {
        snprintf(history_expression, expression_size, "%s %c %s =", left_text, operation, right_text);
    }
    format_number(answer, history_result, result_size);
    copy_text(state->display, sizeof(state->display), history_result);
    copy_text(state->expression, sizeof(state->expression), history_expression);
    state->last_operator = operation;
    state->last_operand = right;
    state->pending_operator = 0;
    state->has_accumulator = 0;
    state->new_input = 1;
    copy_text(state->chain, sizeof(state->chain), history_expression);
    append_text(state->chain, sizeof(state->chain), " ");
    append_text(state->chain, sizeof(state->chain), history_result);
    state->restored_history = 0;
    state->continuation_available = 1;
    return 1;
}

int calc_unary(CalcState *state, char operation, char *history_expression,
               size_t expression_size, char *history_result, size_t result_size) {
    double input, answer;
    char input_text[96];
    if (state->error) return 0;
    input = calc_value(state);
    format_number(input, input_text, sizeof(input_text));
    switch (operation) {
        case 'r':
            if (input == 0.0) {
                set_error(state, "Cannot divide by zero");
                return 0;
            }
            answer = 1.0 / input;
            snprintf(history_expression, expression_size, "1/(%s) =", input_text);
            break;
        case 's':
            answer = input * input;
            snprintf(history_expression, expression_size, "sqr(%s) =", input_text);
            break;
        case 'q':
            if (input < 0.0) {
                set_error(state, "Invalid input");
                return 0;
            }
            answer = sqrt(input);
            snprintf(history_expression, expression_size, "sqrt(%s) =", input_text);
            break;
        default: return 0;
    }
    if (!isfinite(answer)) {
        set_error(state, "Overflow");
        return 0;
    }
    format_number(answer, history_result, result_size);
    copy_text(state->display, sizeof(state->display), history_result);
    if (state->pending_operator) {
        state->new_input = 0;
        update_expression_preview(state);
    } else {
        copy_text(state->expression, sizeof(state->expression), history_expression);
        state->new_input = 1;
        state->chain[0] = '\0';
    }
    state->restored_history = 0;
    state->continuation_available = 0;
    return 1;
}

void calc_percent(CalcState *state) {
    double input, result;
    char input_text[96];
    if (state->error) return;
    input = calc_value(state);
    if (!state->pending_operator) {
        /* Google-style percentage-of input: 20 % 100 = 20.  Keep the
           percentage visible in the expression while arming an implicit
           multiplication by the amount entered next. */
        copy_text(input_text, sizeof(input_text), state->display);
        state->accumulator = input / 100.0;
        state->has_accumulator = 1;
        state->pending_operator = '*';
        state->last_operator = 0;
        state->new_input = 1;
        state->chain[0] = '\0';
        append_text(state->chain, sizeof(state->chain), input_text);
        append_text(state->chain, sizeof(state->chain), "% * ");
        copy_text(state->expression, sizeof(state->expression), state->chain);
        state->restored_history = 0;
        state->continuation_available = 0;
        return;
    }
    if (state->new_input) return;
    if (state->pending_operator == '+' || state->pending_operator == '-') {
        result = state->accumulator * input / 100.0;
    } else {
        result = input / 100.0;
    }
    format_number(result, state->display, sizeof(state->display));
    state->new_input = 0;
    update_expression_preview(state);
}

void calc_memory_clear(CalcState *state) {
    state->has_memory = 0;
    state->memory = 0.0;
}

void calc_memory_recall(CalcState *state) {
    if (!state->has_memory) return;
    calc_set_value(state, state->memory);
}

void calc_memory_store(CalcState *state) {
    if (state->error) return;
    state->memory = calc_value(state);
    state->has_memory = 1;
}

void calc_memory_add(CalcState *state) {
    if (state->error) return;
    state->memory = (state->has_memory ? state->memory : 0.0) + calc_value(state);
    state->has_memory = 1;
}

void calc_memory_subtract(CalcState *state) {
    if (state->error) return;
    state->memory = (state->has_memory ? state->memory : 0.0) - calc_value(state);
    state->has_memory = 1;
}
