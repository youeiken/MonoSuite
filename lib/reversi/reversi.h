#ifndef REVERSI_H
#define REVERSI_H

// #include "appmode.h"
// #include "appstate.h"
// #include "display.h"
// #include "storage.h"
// #include "touch.h"
#include <M5Unified.h>
#include <vector>

// Constants for 480x800 display
#define BLACK 1
#define WHITE 2
#define CELL 60
#define BOARD_TOP 80
#define BOARD_BOTTOM 480
#define PIECE_R 24
#define AI_DEPTH 4

// Game state variables - declared as extern
extern int board[64];
extern int turn;
extern bool rs_gameOver;
extern bool rs_initialized;
extern unsigned long rs_lastActivity;
extern std::vector<int> history;

// Function declarations
void initReversi();
void newReversiGame();
void restartReversiGame();
void redrawReversi(const char* status = nullptr);
void redrawReversiQuality();
void reversiLoop();

void initBoard();
bool isValidMove(int b[64], int p, int r, int c);
void getMoves(int b[64], int p, std::vector<int>& moves);
void applyMove(int b[64], int p, int r, int c);
int evaluate(int b[64]);
int terminalScore(int b[64]);
void search(int b[64], int p, int depth, int alpha, int beta, int& bestMove, int& bestScore);
void playAI();
void onHumanPlace(int r, int c);
void onHumanPass();
void onUndo();
void drawBoardReversi();
void drawHUDReversi(const char* status = nullptr);
// void drawBattery();
void posToCell(int x, int y, int& r, int& c);
bool isInBoard(int x, int y);

// External
// extern void enterLauncher();
extern void showInfoModalAndWait(const char* title, const char* const* lines, int lineCount);

#endif
