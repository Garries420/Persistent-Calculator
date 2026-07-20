#ifndef UPDATER_H
#define UPDATER_H

#include <windows.h>

#define PERSISTENT_CALCULATOR_VERSION_A "1.0.0"
#define PERSISTENT_CALCULATOR_VERSION_W L"1.0.0"

int updater_apply_if_requested(void);
void updater_initialize(HWND owner);
void updater_check_now(HWND owner);
void updater_shutdown(void);

#endif
