#ifndef SOKOBAN_H
#define SOKOBAN_H

// #include "appmode.h"
// #include "appstate.h"
// #include "display.h"
// #include "storage.h"
// #include "touch.h"
#include <M5Unified.h>

// Grid dimensions — must accommodate the largest level we ship
#define SK_MAX_COLS   56
#define SK_MAX_ROWS   32
#define SK_BOARD_LEFT 0
#define SK_BOARD_TOP  80

// Runtime cell size (auto-scaled per level)
extern int sk_cell;

// Cell contents
#define SK_FLOOR    0
#define SK_WALL     1
#define SK_GOAL     2

// Entity flags (separate from floor)
#define SK_BOX      0x10
#define SK_PLAYER   0x20

// Game state - declared as extern
extern bool sk_initialized;
extern bool sk_won;
extern bool sk_deadlocked;
extern int  sk_levelIndex;
extern int  sk_moveCount;
extern int  sk_pushCount;
extern unsigned long sk_lastActivity;

// Public API
void initSokoban();
void newSokobanGame();
void restartSokobanGame();
void redrawSokoban(const char* status = nullptr);
void redrawSokobanQuality();
void sokobanLoop();
void loadNextSokobanLevel();

// External
// extern void enterLauncher();
extern void showInfoModalAndWait(const char* title, const char* const* lines, int lineCount);

#endif
