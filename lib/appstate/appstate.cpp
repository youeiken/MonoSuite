#include "appstate.h"

// ============================================================
// Sleep flag
// ============================================================
static bool s_sleeping = false;

bool isSleeping()                  { return s_sleeping; }
void setSleeping(bool sleeping)    { s_sleeping = sleeping; }

// ============================================================
// Activity timestamp
// ============================================================
static unsigned long s_lastActivityMs = 0;

unsigned long lastActivityMs() {
    return s_lastActivityMs;
}

void touchGlobalActivity() {
    s_lastActivityMs = millis();
}
