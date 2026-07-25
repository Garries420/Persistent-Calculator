#ifndef UPDATER_H
#define UPDATER_H

#include <windows.h>

#define PERSISTENT_CALCULATOR_VERSION_A "2.0.1"
#define PERSISTENT_CALCULATOR_VERSION_W L"2.0.1"

int updater_apply_if_requested(void);
void updater_initialize(HWND owner);
void updater_check_now(HWND owner);
int updater_is_modal(void);
void updater_owner_resized(HWND owner);
void updater_shutdown(void);

#endif
