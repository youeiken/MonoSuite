#include "minesweeper.h"
#include "launcher.h"

#include "appmode.h"
#include "appstate.h"
#include "display.h"
#include "storage.h"
#include "touch.h"
#include "savegame.h"

static void saveProgress();
static bool loadProgress();

// ============================================================
// Game State
// ============================================================

int  ms_grid[MS_ROWS * MS_COLS];
int  ms_state[MS_ROWS * MS_COLS];
bool ms_initialized = false;
bool ms_gameOver = false;
bool ms_won = false;
bool ms_flagMode = false;
int  ms_minesTotal = MS_EASY_MINES;
int  ms_flagsPlaced = 0;
int  ms_cellsRevealed = 0;
unsigned long ms_lastActivity = 0;

static bool minesPlaced = false;

// ============================================================
// Save / Load progress
// ============================================================

static const char* MS_SAVE_PATH = "/minesweeper_save.txt";

static void saveProgress() {
    if (ms_gameOver || ms_won) {
        saveGameDelete(MS_SAVE_PATH);
        return;
    }
    if (!minesPlaced) {
        saveGameDelete(MS_SAVE_PATH);
        return;
    }

    String s = "";
    s += "mines=" + String(ms_minesTotal) + "\n";
    s += "flags=" + String(ms_flagsPlaced) + "\n";
    s += "revealed=" + String(ms_cellsRevealed) + "\n";
    s += "flagMode=" + String(ms_flagMode ? 1 : 0) + "\n";

    s += "grid=";
    for (int i = 0; i < MS_ROWS * MS_COLS; i++) {
        s += String(ms_grid[i]);
        if (i < MS_ROWS * MS_COLS - 1) s += ",";
    }
    s += "\n";

    s += "state=";
    for (int i = 0; i < MS_ROWS * MS_COLS; i++) {
        s += String(ms_state[i]);
        if (i < MS_ROWS * MS_COLS - 1) s += ",";
    }
    s += "\n";

    saveGameWrite(MS_SAVE_PATH, s);
}

static bool loadProgress() {
    if (!saveGameExists(MS_SAVE_PATH)) return false;
    String s = saveGameRead(MS_SAVE_PATH);
    if (s.length() == 0) return false;

    int savedMines = -1;
    int savedFlags = 0;
    int savedRevealed = 0;
    bool savedFlagMode = false;
    int savedGrid[MS_ROWS * MS_COLS];
    int savedState[MS_ROWS * MS_COLS];
    bool gotGrid = false, gotState = false;
    for (int i = 0; i < MS_ROWS * MS_COLS; i++) {
        savedGrid[i] = 0;
        savedState[i] = MS_HIDDEN;
    }

    int lineStart = 0;
    while (lineStart < s.length()) {
        int lineEnd = s.indexOf('\n', lineStart);
        if (lineEnd < 0) lineEnd = s.length();
        String line = s.substring(lineStart, lineEnd);
        line.trim();

        if (line.startsWith("mines=")) {
            savedMines = line.substring(6).toInt();
        } else if (line.startsWith("flags=")) {
            savedFlags = line.substring(6).toInt();
        } else if (line.startsWith("revealed=")) {
            savedRevealed = line.substring(9).toInt();
        } else if (line.startsWith("flagMode=")) {
            savedFlagMode = line.substring(9).toInt() != 0;
        } else if (line.startsWith("grid=")) {
            String csv = line.substring(5);
            int idx = 0, start = 0;
            while (idx < MS_ROWS * MS_COLS && start < (int)csv.length()) {
                int comma = csv.indexOf(',', start);
                if (comma < 0) comma = csv.length();
                savedGrid[idx++] = csv.substring(start, comma).toInt();
                start = comma + 1;
            }
            if (idx == MS_ROWS * MS_COLS) gotGrid = true;
        } else if (line.startsWith("state=")) {
            String csv = line.substring(6);
            int idx = 0, start = 0;
            while (idx < MS_ROWS * MS_COLS && start < (int)csv.length()) {
                int comma = csv.indexOf(',', start);
                if (comma < 0) comma = csv.length();
                savedState[idx++] = csv.substring(start, comma).toInt();
                start = comma + 1;
            }
            if (idx == MS_ROWS * MS_COLS) gotState = true;
        }
        lineStart = lineEnd + 1;
    }

    if (savedMines <= 0) return false;
    if (!gotGrid || !gotState) return false;

    int mineCount = 0;
    for (int i = 0; i < MS_ROWS * MS_COLS; i++) {
        if (savedGrid[i] == 1) mineCount++;
    }
    if (mineCount != savedMines) return false;

    for (int i = 0; i < MS_ROWS * MS_COLS; i++) {
        ms_grid[i] = savedGrid[i];
        ms_state[i] = savedState[i];
    }
    ms_minesTotal = savedMines;
    ms_flagsPlaced = savedFlags;
    ms_cellsRevealed = savedRevealed;
    ms_flagMode = savedFlagMode;
    ms_gameOver = false;
    ms_won = false;
    minesPlaced = true;

    Serial.printf("[Minesweeper] Resumed (mines=%d, revealed=%d)\n",
                  savedMines, savedRevealed);
    return true;
}

// ============================================================
// Core logic
// ============================================================

static inline int idx(int r, int c) { return r * MS_COLS + c; }

static inline bool inBounds(int r, int c) {
    return r >= 0 && r < MS_ROWS && c >= 0 && c < MS_COLS;
}

static int countAdjacentMines(int r, int c) {
    int count = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            int nr = r + dr, nc = c + dc;
            if (inBounds(nr, nc) && ms_grid[idx(nr, nc)] == 1) count++;
        }
    }
    return count;
}

static void placeMines(int safeR, int safeC) {
    for (int i = 0; i < MS_ROWS * MS_COLS; i++) ms_grid[i] = 0;

    int placed = 0;
    while (placed < ms_minesTotal) {
        int r = random(MS_ROWS);
        int c = random(MS_COLS);

        if (abs(r - safeR) <= 1 && abs(c - safeC) <= 1) continue;
        if (ms_grid[idx(r, c)] == 1) continue;

        ms_grid[idx(r, c)] = 1;
        placed++;
    }

    minesPlaced = true;
}

static void revealCell(int r, int c) {
    if (!inBounds(r, c)) return;
    if (ms_state[idx(r, c)] != MS_HIDDEN) return;

    ms_state[idx(r, c)] = MS_REVEALED;
    ms_cellsRevealed++;

    if (ms_grid[idx(r, c)] == 1) {
        ms_gameOver = true;
        return;
    }

    if (countAdjacentMines(r, c) == 0) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                revealCell(r + dr, c + dc);
            }
        }
    }
}

static void toggleFlag(int r, int c) {
    int i = idx(r, c);
    if (ms_state[i] == MS_REVEALED) return;

    if (ms_state[i] == MS_FLAGGED) {
        ms_state[i] = MS_HIDDEN;
        ms_flagsPlaced--;
    } else {
        ms_state[i] = MS_FLAGGED;
        ms_flagsPlaced++;
    }
}

static bool checkWin() {
    int totalCells = MS_ROWS * MS_COLS;
    int nonMineCells = totalCells - ms_minesTotal;
    return ms_cellsRevealed >= nonMineCells;
}

static void revealAllMines() {
    for (int i = 0; i < MS_ROWS * MS_COLS; i++) {
        if (ms_grid[i] == 1) ms_state[i] = MS_REVEALED;
    }
}

// ============================================================
// Drawing
// ============================================================

static void drawHeaderMinesweeper() {
    drawHeader("", true);
}

static void drawCell(int r, int c) {
    int x = MS_BOARD_LEFT + c * MS_CELL;
    int y = MS_BOARD_TOP + r * MS_CELL;
    int i = idx(r, c);

    uint16_t bg;
    if (ms_state[i] == MS_REVEALED) {
        bg = TFT_WHITE;
    } else {
        bg = 0x8410;
    }

    M5.Display.fillRect(x, y, MS_CELL, MS_CELL, bg);
    M5.Display.drawRect(x, y, MS_CELL, MS_CELL, TFT_BLACK);

    if (ms_state[i] == MS_FLAGGED) {
        int cx = x + MS_CELL / 2;
        int cy = y + MS_CELL / 2;
        M5.Display.fillTriangle(
            cx - 4, cy + 8,
            cx + 8, cy,
            cx - 4, cy - 8,
            TFT_BLACK
        );
        M5.Display.drawLine(cx - 4, cy - 8, cx - 4, cy + 8, TFT_BLACK);
        return;
    }

    if (ms_state[i] == MS_REVEALED) {
        if (ms_grid[i] == 1) {
            int cx = x + MS_CELL / 2;
            int cy = y + MS_CELL / 2;
            M5.Display.fillCircle(cx, cy, MS_CELL / 4, TFT_BLACK);
            return;
        }

        int adj = countAdjacentMines(r, c);
        if (adj > 0) {
            M5.Display.setFont(&fonts::FreeSansBold12pt7b);
            M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
            String s = String(adj);
            int tw = M5.Display.textWidth(s);
            int th = M5.Display.fontHeight();
            M5.Display.drawString(s, x + (MS_CELL - tw) / 2, y + (MS_CELL - th) / 2);
        }
    }
}

static void drawHUDMinesweeper(const char* status = nullptr) {
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // Stats
    int remaining = ms_minesTotal - ms_flagsPlaced;
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.drawString("Mines: " + String(remaining) +
                          "   Revealed: " + String(ms_cellsRevealed),
                          20, 630);

    // Status line
    String line;
    if (status != nullptr) {
        line = String(status);
    } else if (ms_won) {
        line = "You cleared the field!";
    } else if (ms_gameOver) {
        line = "Boom! Try again.";
    } else if (ms_flagMode) {
        line = "FLAG MODE - tap to flag";
    } else {
        line = "Have fun!";
    }
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString(line, 20, 670);

    // Instructions
    // M5.Display.setFont(&fonts::FreeSans9pt7b);
    // M5.Display.drawString("Tap to reveal", 20, 710);
    // M5.Display.drawString("Press BtnA = Flag Mode", 20, 740);

    // Version
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString("MINESWEEPER v1.0", 20, 760);
}

void redrawMinesweeperQuality() {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    beginDisplay();
    clearScreen();
    drawHeaderMinesweeper();
    for (int r = 0; r < MS_ROWS; r++)
        for (int c = 0; c < MS_COLS; c++)
            drawCell(r, c);
    drawHUDMinesweeper();
    endDisplay();
}

void redrawMinesweeper(const char* status) {
    static int fastRefreshCount = 0;
    if (fastRefreshCount >= 100) {
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
        fastRefreshCount = 0;
    } else {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        fastRefreshCount++;
    }

    beginDisplay();
    clearScreen();
    drawHeaderMinesweeper();
    for (int r = 0; r < MS_ROWS; r++)
        for (int c = 0; c < MS_COLS; c++)
            drawCell(r, c);
    drawHUDMinesweeper(status);
    endDisplay();
}

// ============================================================
// Public API
// ============================================================

void initMinesweeper() {
    if (loadProgress()) {
        ms_initialized = true;
        ms_lastActivity = millis();
        return;
    }

    for (int i = 0; i < MS_ROWS * MS_COLS; i++) {
        ms_grid[i] = 0;
        ms_state[i] = MS_HIDDEN;
    }
    ms_gameOver = false;
    ms_won = false;
    ms_flagMode = false;
    ms_flagsPlaced = 0;
    ms_cellsRevealed = 0;
    ms_initialized = true;
    ms_lastActivity = millis();
    minesPlaced = false;
}

void newMinesweeperGame() {
    initMinesweeper();
    redrawMinesweeperQuality();
}

void restartMinesweeperGame() {
    saveGameDelete(MS_SAVE_PATH);
    initMinesweeper();
    redrawMinesweeper("New game!");
    delay(400);
    redrawMinesweeperQuality();
}

static bool tapToCell(int x, int y, int& r, int& c) {
    if (x < MS_BOARD_LEFT || x >= MS_BOARD_LEFT + MS_COLS * MS_CELL) return false;
    if (y < MS_BOARD_TOP  || y >= MS_BOARD_TOP  + MS_ROWS * MS_CELL) return false;
    c = (x - MS_BOARD_LEFT) / MS_CELL;
    r = (y - MS_BOARD_TOP)  / MS_CELL;
    return inBounds(r, c);
}

void minesweeperLoop() {
    if (M5.BtnA.wasHold()) {
        if (confirmDialog("Quit?", "Return to the launcher?", "Quit", "Stay")) {
            enterLauncher();
        } else {
            redrawMinesweeperQuality();
        }
        return;
    }

    if (M5.BtnA.wasClicked()) {
        ms_flagMode = !ms_flagMode;
        redrawMinesweeper();
        return;
    }

    if (M5.BtnB.wasHold()) {
        if (confirmDialog("Restart?", "Start a new game?", "Restart", "Cancel")) {
            restartMinesweeperGame();
        } else {
            redrawMinesweeperQuality();
        }
        return;
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
            static const char* const minesweeperHelp[] = {
                "Tap a cell = Reveal",
                "BtnA (short) = Toggle flag mode",
                "",
                "Hold BtnA = Launcher",
                "Hold BtnB = New game",
                "Hold BtnA + BtnB = Sleep / Wake",
            };
            showInfoModalAndWait("MINESWEEPER v1.0", minesweeperHelp, 6);

            // Redraw the minesweeper board
            redrawMinesweeperQuality();
            return;
        }

        int r, c;
        if (tapToCell(x, y, r, c)) {
            int i = idx(r, c);

            if (ms_flagMode) {
                if (ms_state[i] != MS_REVEALED) {
                    toggleFlag(r, c);
                    redrawMinesweeper();
                    saveProgress();
                }
            } else {
                if (ms_state[i] == MS_FLAGGED) {
                    // do nothing
                } else if (ms_state[i] == MS_HIDDEN) {
                    if (!minesPlaced) {
                        placeMines(r, c);
                    }
                    revealCell(r, c);

                    if (ms_grid[i] == 1) {
                        revealAllMines();
                        ms_gameOver = true;
                    } else if (checkWin()) {
                        ms_won = true;
                    }
                    redrawMinesweeper();

                    if (ms_gameOver || ms_won) {
                        saveGameDelete(MS_SAVE_PATH);
                    } else {
                        saveProgress();
                    }
                }
            }
            ms_lastActivity = millis();
        }
        resetTouch();
    }

    if (ms_gameOver || ms_won) return;
}
