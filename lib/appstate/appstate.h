#ifndef APPSTATE_H
#define APPSTATE_H

#include <M5Unified.h>

// ============================================================
// Shared app-wide state.
//
// Definitions live in appstate.cpp. Consumers should use the
// accessors, not the raw variables — the accessors are where
// state transitions stay consistent.
// ============================================================

// ---- Sleep flag ----------------------------------------------------------
bool isSleeping();
void setSleeping(bool sleeping);

// ---- Global last-activity timestamp -------------------------------------
unsigned long lastActivityMs();
void touchGlobalActivity();

// Refreshes the activity timer of the currently-active mode. Declared
// here, defined by main.cpp (it needs to know the current mode).
void touchGameActivity();

#endif
