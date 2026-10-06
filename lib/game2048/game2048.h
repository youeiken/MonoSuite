#ifndef GAME2048_H
#define GAME2048_H

// #include "appmode.h"
// #include "appstate.h"
// #include "display.h"
// #include "storage.h"
// #include "touch.h"
#include <M5Unified.h>

// Board is 4x4, each cell 108x108 px
#define G2048_SIZE 4
#define G2048_CELL 108
#define G2048_BOARD_LEFT 20
#define G2048_BOARD_TOP  80

// Game state - declared as extern
extern int g2048_board[16];
extern int g2048_score;
extern int g2048_best;
extern bool g2048_gameOver;
extern bool g2048_won;
extern bool g2048_initialized;
extern unsigned long g2048_lastActivity;

// Public API
void initGame2048();
void newGame2048();
void restartGame2048();
void redrawGame2048(const char* status = nullptr);
void redrawGame2048Quality();
void game2048Loop();
void drawHUD2048(const char* status = nullptr);

// External
// extern void enterLauncher();
extern void showInfoModalAndWait(const char* title, const char* const* lines, int lineCount);

#endif
