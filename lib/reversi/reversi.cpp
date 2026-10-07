#include "reversi.h"
#include "launcher.h"

#include "appmode.h"
#include "appstate.h"
#include "display.h"
#include "storage.h"
#include "touch.h"
#include "savegame.h"
#include <algorithm>
#include <LittleFS.h>
#include <SD.h>

static void saveProgress();
static bool loadProgress();

// ============================================================
// Game State
// ============================================================

int board[64];
int turn = BLACK;
bool rs_gameOver = false;
bool rs_initialized = false;
unsigned long rs_lastActivity = 0;
std::vector<int> history;

// Direction vectors
const int DIRS[8][2] = {
    {-1, -1}, {-1, 0}, {-1, 1},
    {0, -1},           {0, 1},
    {1, -1},  {1, 0},  {1, 1}
};

const int CORNERS[] = {0, 7, 56, 63};

const int WEIGHTS[64] = {
    120, -20, 20, 5, 5, 20, -20, 120,
    -20, -40, -5, -5, -5, -5, -40, -20,
    20, -5, 15, 3, 3, 15, -5, 20,
    5, -5, 3, 3, 3, 3, -5, 5,
    5, -5, 3, 3, 3, 3, -5, 5,
    20, -5, 15, 3, 3, 15, -5, 20,
    -20, -40, -5, -5, -5, -5, -40, -20,
    120, -20, 20, 5, 5, 20, -20, 120,
};

// ============================================================
// Save / Load progress
// ============================================================

static const char* RS_SAVE_PATH = "/reversi/reversi_save.txt";

static void saveProgress() {
    if (rs_gameOver) {
        saveGameDelete(RS_SAVE_PATH);
        return;
    }

    String s = "";
    s += "turn=" + String(turn) + "\n";
    s += "board=";
    for (int i = 0; i < 64; i++) {
        s += String(board[i]);
        if (i < 63) s += ",";
    }
    s += "\n";
    saveGameWrite(RS_SAVE_PATH, s);
}

static bool loadProgress() {
    if (!saveGameExists(RS_SAVE_PATH)) return false;
    String s = saveGameRead(RS_SAVE_PATH);
    if (s.length() == 0) return false;

    int savedTurn = -1;
    int savedBoard[64];
    for (int i = 0; i < 64; i++) savedBoard[i] = 0;
    bool gotBoard = false;

    int lineStart = 0;
    while (lineStart < s.length()) {
        int lineEnd = s.indexOf('\n', lineStart);
        if (lineEnd < 0) lineEnd = s.length();
        String line = s.substring(lineStart, lineEnd);
        line.trim();

        if (line.startsWith("turn=")) {
            savedTurn = line.substring(5).toInt();
        } else if (line.startsWith("board=")) {
            String csv = line.substring(6);
            int idx = 0;
            int start = 0;
            while (idx < 64 && start < (int)csv.length()) {
                int comma = csv.indexOf(',', start);
                if (comma < 0) comma = csv.length();
                savedBoard[idx++] = csv.substring(start, comma).toInt();
                start = comma + 1;
            }
            if (idx == 64) gotBoard = true;
        }
        lineStart = lineEnd + 1;
    }

    if (!gotBoard) return false;
    if (savedTurn != BLACK && savedTurn != WHITE) return false;

    int blackCount = 0, whiteCount = 0;
    for (int i = 0; i < 64; i++) {
        if (savedBoard[i] == BLACK) blackCount++;
        else if (savedBoard[i] == WHITE) whiteCount++;
    }
    if (blackCount == 0 && whiteCount == 0) return false;

    for (int i = 0; i < 64; i++) board[i] = savedBoard[i];
    turn = savedTurn;
    rs_gameOver = false;
    history.clear();

    Serial.printf("[Reversi] Resumed (turn=%d, B=%d W=%d)\n",
                  turn, blackCount, whiteCount);
    return true;
}

// ============================================================
//  Game Logic
// ============================================================

void initBoard() {
    for (int i = 0; i < 64; i++) board[i] = 0;
    board[3*8 + 3] = WHITE;
    board[3*8 + 4] = BLACK;
    board[4*8 + 3] = BLACK;
    board[4*8 + 4] = WHITE;
}

bool isValidMove(int b[64], int p, int r, int c) {
    if (r < 0 || r >= 8 || c < 0 || c >= 8) return false;
    if (b[r*8 + c] != 0) return false;
    int opp = 3 - p;

    for (int d = 0; d < 8; d++) {
        int dr = DIRS[d][0], dc = DIRS[d][1];
        int nr = r + dr, nc = c + dc;
        bool seen = false;

        while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && b[nr*8 + nc] == opp) {
            seen = true;
            nr += dr;
            nc += dc;
        }

        if (seen && nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && b[nr*8 + nc] == p) {
            return true;
        }
    }
    return false;
}

void getMoves(int b[64], int p, std::vector<int>& moves) {
    moves.clear();
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (b[r*8 + c] == 0 && isValidMove(b, p, r, c)) {
                moves.push_back(r*8 + c);
            }
        }
    }
}

void applyMove(int b[64], int p, int r, int c) {
    int nb[64];
    for (int i = 0; i < 64; i++) nb[i] = b[i];
    nb[r*8 + c] = p;
    int opp = 3 - p;

    for (int d = 0; d < 8; d++) {
        int dr = DIRS[d][0], dc = DIRS[d][1];
        int nr = r + dr, nc = c + dc;
        std::vector<int> line;

        while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && nb[nr*8 + nc] == opp) {
            line.push_back(nr*8 + nc);
            nr += dr;
            nc += dc;
        }

        if (!line.empty() && nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && nb[nr*8 + nc] == p) {
            for (int idx : line) {
                nb[idx] = p;
            }
        }
    }

    for (int i = 0; i < 64; i++) board[i] = nb[i];
}

int evaluate(int b[64]) {
    int s = 0;
    int black = 0, white = 0;
    for (int i = 0; i < 64; i++) {
        int v = b[i];
        if (v == WHITE) {
            s += WEIGHTS[i];
            white++;
        } else if (v == BLACK) {
            s -= WEIGHTS[i];
            black++;
        }
    }
    int total = black + white;
    if (total > 44) {
        s += (white - black) * 10;
    }
    return s;
}

int terminalScore(int b[64]) {
    int w = 0, k = 0;
    for (int i = 0; i < 64; i++) {
        if (b[i] == WHITE) w++;
        else if (b[i] == BLACK) k++;
    }
    if (w > k) return 10000 + (w - k);
    if (w < k) return -10000 - (k - w);
    return 0;
}

int moveKey(int idx) {
    for (int c : CORNERS) {
        if (idx == c) return 0;
    }
    int r = idx / 8, c = idx % 8;
    if (r == 0 || r == 7 || c == 0 || c == 7) return 1;
    return 2;
}

void search(int b[64], int p, int depth, int alpha, int beta, int& bestMove, int& bestScore) {
    yield();

    std::vector<int> moves;
    getMoves(b, p, moves);

    if (moves.empty()) {
        int opp = 3 - p;
        std::vector<int> oppMoves;
        getMoves(b, opp, oppMoves);
        if (oppMoves.empty()) {
            bestMove = -1;
            bestScore = terminalScore(b);
            return;
        }
        search(b, opp, depth, alpha, beta, bestMove, bestScore);
        return;
    }

    if (depth == 0) {
        bestMove = moves[0];
        bestScore = evaluate(b);
        return;
    }

    std::sort(moves.begin(), moves.end(), [](int a, int b) {
        return moveKey(a) < moveKey(b);
    });

    bestMove = moves[0];

    if (p == WHITE) {
        bestScore = -1000000;
        for (int idx : moves) {
            yield();

            int r = idx / 8, c = idx % 8;
            int nb[64];
            for (int i = 0; i < 64; i++) nb[i] = b[i];
            nb[r*8 + c] = p;

            int childMove, childScore;
            search(nb, 3 - p, depth - 1, alpha, beta, childMove, childScore);

            if (childScore > bestScore) {
                bestScore = childScore;
                bestMove = idx;
            }
            if (bestScore > alpha) alpha = bestScore;
            if (alpha >= beta) break;
        }
    } else {
        bestScore = 1000000;
        for (int idx : moves) {
            yield();

            int r = idx / 8, c = idx % 8;
            int nb[64];
            for (int i = 0; i < 64; i++) nb[i] = b[i];
            nb[r*8 + c] = p;

            int childMove, childScore;
            search(nb, 3 - p, depth - 1, alpha, beta, childMove, childScore);

            if (childScore < bestScore) {
                bestScore = childScore;
                bestMove = idx;
            }
            if (bestScore < beta) beta = bestScore;
            if (alpha >= beta) break;
        }
    }
}

// ============================================================
//  AI Play
// ============================================================

void playAI() {
    while (true) {
        if (turn != WHITE) return;

        std::vector<int> moves;
        getMoves(board, WHITE, moves);

        if (!moves.empty()) {
            int bestMove, bestScore;
            search(board, WHITE, AI_DEPTH, -1000000, 1000000, bestMove, bestScore);
            if (bestMove == -1) bestMove = moves[0];

            int r = bestMove / 8, c = bestMove % 8;
            applyMove(board, WHITE, r, c);
            turn = BLACK;

            delay(100);
        } else {
            turn = BLACK;
        }

        std::vector<int> blackMoves;
        getMoves(board, BLACK, blackMoves);
        if (!blackMoves.empty()) return;

        std::vector<int> whiteMoves;
        getMoves(board, WHITE, whiteMoves);
        if (whiteMoves.empty()) {
            rs_gameOver = true;
            turn = 0;
            return;
        }
        turn = WHITE;
    }
}

// ============================================================
//  Human Actions
// ============================================================

void onHumanPlace(int r, int c) {
    if (history.size() > 64 * 20) {
        history.erase(history.begin(), history.begin() + 64);
    }
    for (int i = 0; i < 64; i++) history.push_back(board[i]);

    applyMove(board, BLACK, r, c);
    turn = WHITE;
    redrawReversi("AI thinking...");
    delay(200);
    playAI();
    redrawReversi();

    if (rs_gameOver) {
        saveGameDelete(RS_SAVE_PATH);
    } else {
        saveProgress();
    }
}

void onHumanPass() {
    for (int i = 0; i < 64; i++) history.push_back(board[i]);
    turn = WHITE;
    redrawReversi("You pass");
    delay(200);
    playAI();
    redrawReversi();

    if (rs_gameOver) {
        saveGameDelete(RS_SAVE_PATH);
    } else {
        saveProgress();
    }
}

void onUndo() {
    if (history.size() >= 64) {
        for (int i = 0; i < 64; i++) {
            board[i] = history[history.size() - 64 + i];
        }
        history.resize(history.size() - 64);
        turn = BLACK;
        rs_gameOver = false;
        redrawReversi();
    }
}

// ============================================================
//  Initialization
// ============================================================

void initReversi() {
    if (loadProgress()) {
        rs_initialized = true;
        rs_lastActivity = millis();
        return;
    }

    initBoard();
    turn = BLACK;
    rs_gameOver = false;
    rs_initialized = true;
    rs_lastActivity = millis();
    history.clear();
    saveProgress();
}

void newReversiGame() {
    initReversi();
    redrawReversiQuality();
}

// ============================================================
//  Restart Game
// ============================================================

void restartReversiGame() {
    saveGameDelete(RS_SAVE_PATH);
    history.clear();
    initBoard();
    turn = BLACK;
    rs_gameOver = false;
    rs_initialized = true;
    rs_lastActivity = millis();
    redrawReversi("New Game!");
    delay(500);
    redrawReversiQuality();
    saveProgress();
}

// ============================================================
//  Drawing
// ============================================================

static void drawHeaderReversi() {
    drawHeader("", true);
}

void drawBoardReversi() {
    // Grid lines
    for (int i = 0; i <= 8; i++) {
        int x = i * CELL;
        int y = BOARD_TOP + i * CELL;

        if (x <= 8 * CELL) {
            M5.Display.drawLine(x, BOARD_TOP, x, BOARD_TOP + 8 * CELL, TFT_BLACK);
        }
        if (y <= BOARD_TOP + 8 * CELL) {
            M5.Display.drawLine(0, y, 8 * CELL, y, TFT_BLACK);
        }
    }

    // Pieces
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int v = board[r*8 + c];
            if (v != 0) {
                int cx = c * CELL + CELL/2;
                int cy = BOARD_TOP + r * CELL + CELL/2;

                if (v == BLACK) {
                    M5.Display.fillCircle(cx, cy, PIECE_R, TFT_BLACK);
                } else {
                    M5.Display.fillCircle(cx, cy, PIECE_R, TFT_WHITE);
                    M5.Display.drawCircle(cx, cy, PIECE_R, TFT_BLACK);
                }
            }
        }
    }

    // Legal-move hints
    if (!rs_gameOver && turn == BLACK) {
        std::vector<int> moves;
        getMoves(board, BLACK, moves);
        for (int idx : moves) {
            int r = idx / 8, c = idx % 8;
            int cx = c * CELL + CELL/2;
            int cy = BOARD_TOP + r * CELL + CELL/2;
            M5.Display.drawCircle(cx, cy, 8, TFT_BLACK);
        }
    }
}

String gameOverText() {
    int k = 0, w = 0;
    for (int i = 0; i < 64; i++) {
        if (board[i] == BLACK) k++;
        else if (board[i] == WHITE) w++;
    }
    if (k > w) return "You win!  " + String(k) + " : " + String(w);
    if (w > k) return "AI wins!  " + String(w) + " : " + String(k);
    return "Draw!  " + String(k) + " : " + String(w);
}

void drawHUDReversi(const char* status) {
    int k = 0, w = 0;
    for (int i = 0; i < 64; i++) {
        if (board[i] == BLACK) k++;
        else if (board[i] == WHITE) w++;
    }

    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // Score
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.drawString("Human " + String(k) + "  :  " + String(w) + " AI", 20, 580);

    // Status
    String line;
    if (status != nullptr) {
        line = String(status);
    } else if (rs_gameOver) {
        line = gameOverText();
    } else if (turn == BLACK) {
        line = "Human's turn";
    } else {
        line = "AI thinking...";
    }
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString(line, 20, 620);

    // Instructions
    // M5.Display.setFont(&fonts::FreeSans9pt7b);
    // M5.Display.drawString("Tap a Cell to place", 20, 680);
    // M5.Display.drawString("Press BtnA = Pass", 20, 710);
    // M5.Display.drawString("Press BtnB = Undo", 20, 740);

    // Version
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString("REVERSI v1.0", 20, 760);
}

void ledYourTurn() {
    M5.Led.setAllColor(0, 0, 255);
    M5.Led.display();
}

void ledAITurn() {
    M5.Led.setAllColor(255, 0, 0);
    M5.Led.display();
}

void ledOff() {
    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();
}

void redrawReversiQuality() {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    beginDisplay();
    clearScreen();
    drawHeaderReversi();
    drawBoardReversi();
    drawHUDReversi();
    endDisplay();
}

void redrawReversi(const char* status) {
    static int fastRefreshCount = 0;

    if (fastRefreshCount >= 100) {
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
        fastRefreshCount = 0;
    } else {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        fastRefreshCount++;
    }

    if (rs_gameOver) {
        ledOff();
    } else if (turn == BLACK) {
        ledYourTurn();
    } else {
        ledAITurn();
    }

    beginDisplay();
    clearScreen();
    drawHeaderReversi();
    drawBoardReversi();
    drawHUDReversi(status);
    endDisplay();
}

// ============================================================
//  Touch Handling
// ============================================================

void posToCell(int x, int y, int& r, int& c) {
    r = -1;
    c = -1;

    int by = y - BOARD_TOP;
    if (x >= 0 && x < 8 * CELL && by >= 0 && by < 8 * CELL) {
        r = by / CELL;
        c = x / CELL;
        if (r >= 8) r = 7;
        if (c >= 8) c = 7;
        if (r < 0) r = 0;
        if (c < 0) c = 0;
    }
}

bool isInBoard(int x, int y) {
    return (x >= 0 && x < 8 * CELL &&
            y >= BOARD_TOP && y < BOARD_TOP + 8 * CELL);
}

// ============================================================
//  Game Loop
// ============================================================

void reversiLoop() {
    if (M5.BtnA.wasHold()) {
        if (confirmDialog("Quit?", "Return to the launcher?", "Quit", "Stay")) {
            enterLauncher();
        } else {
            redrawReversiQuality();
        }
        return;
    }
    
    if (M5.BtnA.wasClicked()) {
        Serial.println("Reversi: Click A -> Pass");
        if (!rs_gameOver) {
            std::vector<int> moves;
            getMoves(board, BLACK, moves);
            if (moves.empty()) {
                onHumanPass();
            } else {
                redrawReversi("You have a legal move!");
                delay(500);
                redrawReversi();
            }
            rs_lastActivity = millis();
        }
    }

    if (M5.BtnB.wasHold()) {
        if (confirmDialog("Restart?", "Start a new game?", "Restart", "Cancel")) {
            restartReversiGame();
        } else {
            redrawReversiQuality();
        }
        return;
    }

    if (M5.BtnB.wasClicked()) {
        Serial.println("Reversi: Click B -> Undo");
        if (!rs_gameOver) {
            onUndo();
            rs_lastActivity = millis();
        }
    }

    int x, y;
    if (getTouch(x, y)) {
        if (hitBackButton(x, y)) {
            enterLauncher();
            resetTouch();
            return;
        }
        // Info button → show modal
        if (hitInfoButton(x, y)) {
            static const char* const reversiHelp[] = {
                "Tap a cell = Place a piece",
                "BtnA (short) = Pass",
                "BtnB (short) = Undo",
                "",
                "Hold BtnA = Launcher",
                "Hold BtnB = New game",
                "Hold BtnA + BtnB = Sleep / Wake",
            };
            showInfoModalAndWait("REVERSI v1.0", reversiHelp, 7);

            // Redraw the reversi board
            redrawReversiQuality();
            return;
        }

        int r, c;
        posToCell(x, y, r, c);

        if (r >= 0 && r < 8 && c >= 0 && c < 8) {
            if (isValidMove(board, BLACK, r, c)) {
                onHumanPlace(r, c);
                rs_lastActivity = millis();
            } else {
                redrawReversi("Invalid move!");
                delay(300);
                redrawReversi();
            }
        }

        resetTouch();
    }

    if (rs_gameOver || turn != BLACK) return;
}
