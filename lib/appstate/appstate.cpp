#include "appstate.h"

// ============================================================
// Screen brightness
// ============================================================
static int s_brightness = 96;   // matches FRONT_LIGHT_DEFAULT

int brightnessLevel() {
    return s_brightness;
}

void setBrightnessLevel(int level) {
    if (level < 0)   level = 0;
    if (level > 255) level = 255;
    s_brightness = level;
    M5.Display.setBrightness(s_brightness);
}

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
