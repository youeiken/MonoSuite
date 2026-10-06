#ifndef MINESWEEPER_H
#define MINESWEEPER_H

// #include "appmode.h"
// #include "appstate.h"
// #include "display.h"
// #include "storage.h"
// #include "touch.h"
#include <M5Unified.h>

// Grid: 12 x 14 cells, 38x38 each = 456 x 532 board
#define MS_COLS        12
#define MS_ROWS        14
#define MS_CELL        38
#define MS_BOARD_LEFT  12
#define MS_BOARD_TOP   80

// Difficulty
#define MS_EASY_MINES   20
#define MS_MEDIUM_MINES 35
#define MS_HARD_MINES   55

// Cell state flags
#define MS_HIDDEN   0
#define MS_REVEALED 1
#define MS_FLAGGED  2

// Game state - declared as extern
extern int  ms_grid[MS_ROWS * MS_COLS];
extern int  ms_state[MS_ROWS * MS_COLS];
extern bool ms_initialized;
extern bool ms_gameOver;
extern bool ms_won;
extern bool ms_flagMode;
extern int  ms_minesTotal;
extern int  ms_flagsPlaced;
extern int  ms_cellsRevealed;
extern unsigned long ms_lastActivity;

// Public API
void initMinesweeper();
void newMinesweeperGame();
void restartMinesweeperGame();
void redrawMinesweeper(const char* status = nullptr);
void redrawMinesweeperQuality();
void minesweeperLoop();

// External
// extern void enterLauncher();
extern void showInfoModalAndWait(const char* title, const char* const* lines, int lineCount);

#endif
