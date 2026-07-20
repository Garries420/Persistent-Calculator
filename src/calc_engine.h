#ifndef CALC_ENGINE_H
#define CALC_ENGINE_H

#include <stddef.h>

typedef struct CalcState {
    char display[96];
    char expression[2048];
    char chain[2048];
    double accumulator;
    double last_operand;
    double memory;
    char pending_operator;
    char last_operator;
    int has_accumulator;
    int has_memory;
    int new_input;
    int error;
    int restored_history;
    int continuation_available;
} CalcState;

void calc_init(CalcState *state);
void calc_digit(CalcState *state, int digit);
void calc_decimal(CalcState *state);
void calc_toggle_sign(CalcState *state);
void calc_backspace(CalcState *state);
void calc_clear_entry(CalcState *state);
void calc_clear_all(CalcState *state);
void calc_set_operator(CalcState *state, char operation);
int calc_equals(CalcState *state, char *history_expression, size_t expression_size,
                char *history_result, size_t result_size);
int calc_unary(CalcState *state, char operation, char *history_expression,
               size_t expression_size, char *history_result, size_t result_size);
void calc_percent(CalcState *state);
void calc_set_value(CalcState *state, double value);
void calc_restore_history(CalcState *state, double value, const char *expression);
double calc_value(const CalcState *state);
void calc_memory_clear(CalcState *state);
void calc_memory_recall(CalcState *state);
void calc_memory_store(CalcState *state);
void calc_memory_add(CalcState *state);
void calc_memory_subtract(CalcState *state);

#endif
