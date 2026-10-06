#ifndef LAUNCHER_H
#define LAUNCHER_H

// #include "appmode.h"
// #include "appstate.h"
// #include "display.h"
// #include "storage.h"
// #include "touch.h"
#include <M5Unified.h>

// Function declarations (defined in launcher.cpp)
void drawLauncher();
void launcherLoop();
void drawButton(int x, int y, int w, int h, const char* title, const char* sub);
void drawBattery();
void drawHeaderClock();

// Back button — drawn by drawHeaderWithBack(), hit-tested per app
void drawBackButton();
bool hitBackButton(int x, int y);

// Draw the standard header. If `withBack` is true, a back button is
// drawn on the left and the title is shifted right to make room.
void drawHeader(const char* title, bool withBack = false);
void drawHeader(const String& title, bool withBack = false);
void drawHeader(const String& title, const lgfx::IFont* font, bool withBack = false);
void showInfoModal(const char* title, const char* const* lines, int lineCount);
bool hitInfoButton(int x, int y);

// Modal confirm dialog. Returns true if the user tapped YES,
// false if they tapped NO or dismissed by tapping outside.
// `message` may be up to 2 lines; keep each line <= ~30 chars
// to fit at the default 12pt font.
bool confirmDialog(const char* title,
                   const char* message,
                   const char* yesLabel = "Yes",
                   const char* noLabel  = "No");

#endif
