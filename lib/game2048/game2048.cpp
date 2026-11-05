#include "game2048.h"
#include "launcher.h"

#include "appmode.h"
#include "appstate.h"
#include "display.h"
#include "storage.h"
#include "touch.h"
#include "savegame.h"
#include <algorithm>

// ============================================================
// Game State
// ============================================================

int g2048_board[16];
int g2048_score = 0;
int g2048_best = 0;
bool g2048_gameOver = false;
bool g2048_won = false;
bool g2048_initialized = false;
unsigned long g2048_lastActivity = 0;

static const char* BEST_SCORE_PATH = "/game2048/2048_best.txt";

// ============================================================
// Best score persistence
// ============================================================

static void loadBestScore() {
    g2048_best = 0;

    File f;
    if (storageOpenRead(BEST_SCORE_PATH, f)) {
        String s = f.readString();
        f.close();
        s.trim();
        g2048_best = s.toInt();
    }
}

static void saveBestScore() {
    if (g2048_score <= g2048_best) return;
    g2048_best = g2048_score;

    File f;
    if (storageOpenWrite(BEST_SCORE_PATH, f)) {
        f.print(g2048_best);
        f.close();
    }
}

// ============================================================
// Save / Load progress
// ============================================================

static const char* G2048_SAVE_PATH = "/game2048/2048_save.txt";

static void saveProgress() {
    if (g2048_gameOver) {
        saveGameDelete(G2048_SAVE_PATH);
        return;
    }

    String s = "";
    s += "score=" + String(g2048_score) + "\n";
    s += "board=";
    for (int i = 0; i < 16; i++) {
        s += String(g2048_board[i]);
        if (i < 15) s += ",";
    }
    s += "\n";
    saveGameWrite(G2048_SAVE_PATH, s);
}

static bool loadProgress() {
    if (!saveGameExists(G2048_SAVE_PATH)) return false;
    String s = saveGameRead(G2048_SAVE_PATH);
    if (s.length() == 0) return false;

    g2048_score = 0;
    for (int i = 0; i < 16; i++) g2048_board[i] = 0;

    int lineStart = 0;
    while (lineStart < s.length()) {
        int lineEnd = s.indexOf('\n', lineStart);
        if (lineEnd < 0) lineEnd = s.length();
        String line = s.substring(lineStart, lineEnd);
        line.trim();

        if (line.startsWith("score=")) {
            g2048_score = line.substring(6).toInt();
        } else if (line.startsWith("board=")) {
            String csv = line.substring(6);
            int idx = 0;
            int start = 0;
            while (idx < 16 && start < (int)csv.length()) {
                int comma = csv.indexOf(',', start);
                if (comma < 0) comma = csv.length();
                g2048_board[idx++] = csv.substring(start, comma).toInt();
                start = comma + 1;
            }
        }
        lineStart = lineEnd + 1;
    }

    bool hasTile = false;
    for (int i = 0; i < 16; i++) {
        if (g2048_board[i] != 0) { hasTile = true; break; }
    }
    return hasTile;
}

// ============================================================
// Core logic
// ============================================================

static int boardIndex(int r, int c) {
    return r * G2048_SIZE + c;
}

static void clearBoard() {
    for (int i = 0; i < 16; i++) g2048_board[i] = 0;
}

static bool boardHasEmpty() {
    for (int i = 0; i < 16; i++) {
        if (g2048_board[i] == 0) return true;
    }
    return false;
}

static void spawnTile() {
    int empties[16];
    int n = 0;
    for (int i = 0; i < 16; i++) {
        if (g2048_board[i] == 0) empties[n++] = i;
    }
    if (n == 0) return;

    int idx = empties[random(n)];
    g2048_board[idx] = (random(10) == 0) ? 4 : 2;
}

static int slideRowLeft(int* row) {
    int out[G2048_SIZE] = {0, 0, 0, 0};
    int write = 0;
    int points = 0;

    for (int i = 0; i < G2048_SIZE; i++) {
        if (row[i] != 0) out[write++] = row[i];
    }

    for (int i = 0; i < G2048_SIZE - 1; i++) {
        if (out[i] != 0 && out[i] == out[i + 1]) {
            out[i] *= 2;
            points += out[i];
            for (int j = i + 1; j < G2048_SIZE - 1; j++) {
                out[j] = out[j + 1];
            }
            out[G2048_SIZE - 1] = 0;
        }
    }

    for (int i = 0; i < G2048_SIZE; i++) row[i] = out[i];
    return points;
}

static bool moveLeft() {
    bool moved = false;
    for (int r = 0; r < G2048_SIZE; r++) {
        int row[G2048_SIZE];
        for (int c = 0; c < G2048_SIZE; c++) row[c] = g2048_board[boardIndex(r, c)];

        int before[G2048_SIZE];
        for (int c = 0; c < G2048_SIZE; c++) before[c] = row[c];

        int pts = slideRowLeft(row);
        g2048_score += pts;

        for (int c = 0; c < G2048_SIZE; c++) {
            if (row[c] != before[c]) moved = true;
            g2048_board[boardIndex(r, c)] = row[c];
        }
    }
    return moved;
}

static bool moveRight() {
    bool moved = false;
    for (int r = 0; r < G2048_SIZE; r++) {
        int row[G2048_SIZE];
        for (int c = 0; c < G2048_SIZE; c++) row[c] = g2048_board[boardIndex(r, G2048_SIZE - 1 - c)];

        int before[G2048_SIZE];
        for (int c = 0; c < G2048_SIZE; c++) before[c] = row[c];

        int pts = slideRowLeft(row);
        g2048_score += pts;

        for (int c = 0; c < G2048_SIZE; c++) {
            if (row[c] != before[c]) moved = true;
            g2048_board[boardIndex(r, G2048_SIZE - 1 - c)] = row[c];
        }
    }
    return moved;
}

static bool moveUp() {
    bool moved = false;
    for (int c = 0; c < G2048_SIZE; c++) {
        int row[G2048_SIZE];
        for (int r = 0; r < G2048_SIZE; r++) row[r] = g2048_board[boardIndex(r, c)];

        int before[G2048_SIZE];
        for (int r = 0; r < G2048_SIZE; r++) before[r] = row[r];

        int pts = slideRowLeft(row);
        g2048_score += pts;

        for (int r = 0; r < G2048_SIZE; r++) {
            if (row[r] != before[r]) moved = true;
            g2048_board[boardIndex(r, c)] = row[r];
        }
    }
    return moved;
}

static bool moveDown() {
    bool moved = false;
    for (int c = 0; c < G2048_SIZE; c++) {
        int row[G2048_SIZE];
        for (int r = 0; r < G2048_SIZE; r++) row[r] = g2048_board[boardIndex(G2048_SIZE - 1 - r, c)];

        int before[G2048_SIZE];
        for (int r = 0; r < G2048_SIZE; r++) before[r] = row[r];

        int pts = slideRowLeft(row);
        g2048_score += pts;

        for (int r = 0; r < G2048_SIZE; r++) {
            if (row[r] != before[r]) moved = true;
            g2048_board[boardIndex(G2048_SIZE - 1 - r, c)] = row[r];
        }
    }
    return moved;
}

static bool canMoveAnyDirection() {
    if (boardHasEmpty()) return true;
    for (int r = 0; r < G2048_SIZE; r++) {
        for (int c = 0; c < G2048_SIZE; c++) {
            int v = g2048_board[boardIndex(r, c)];
            if (c + 1 < G2048_SIZE && g2048_board[boardIndex(r, c + 1)] == v) return true;
            if (r + 1 < G2048_SIZE && g2048_board[boardIndex(r + 1, c)] == v) return true;
        }
    }
    return false;
}

static void checkWin() {
    for (int i = 0; i < 16; i++) {
        if (g2048_board[i] >= 2048) {
            g2048_won = true;
            return;
        }
    }
}

static bool tryMove(int dir) {
    if (g2048_gameOver) return false;

    bool moved = false;
    switch (dir) {
        case 0: moved = moveUp();    break;
        case 1: moved = moveDown();  break;
        case 2: moved = moveLeft();  break;
        case 3: moved = moveRight(); break;
    }

    if (moved) {
        spawnTile();
        checkWin();
        saveBestScore();

        if (!canMoveAnyDirection()) {
            g2048_gameOver = true;
        }
        saveProgress();
    }
    return moved;
}

// ============================================================
// Drawing
// ============================================================

static uint16_t tileColor(int value) {
    switch (value) {
        case 0:    return 0xFFFF;
        case 2:    return 0xFFFF;
        case 4:    return 0xFFFF;
        case 8:    return 0x8410;
        case 16:   return 0x8410;
        case 32:   return 0x4208;
        case 64:   return 0x4208;
        case 128:  return 0x0000;
        default:   return 0x0000;
    }
}

static uint16_t tileTextColor(int value) {
    switch (value) {
        case 0:  return 0x0000;
        case 2:  return 0x0000;
        case 4:  return 0x0000;
        case 8:  return 0x0000;
        case 16: return 0x0000;
        case 32: return 0xFFFF;
        case 64: return 0xFFFF;
        default: return 0xFFFF;
    }
}

static const lgfx::IFont* tileFont(int value) {
    (void)value;
    return &fonts::FreeSansBold18pt7b;
}

static void drawHeader2048() {
    drawHeader("", true);
}

void drawBoard2048() {
    for (int r = 0; r < G2048_SIZE; r++) {
        for (int c = 0; c < G2048_SIZE; c++) {
            int v = g2048_board[boardIndex(r, c)];
            int x = G2048_BOARD_LEFT + c * G2048_CELL;
            int y = G2048_BOARD_TOP + r * G2048_CELL;

            uint16_t bg = tileColor(v);
            M5.Display.fillRect(x + 3, y + 3, G2048_CELL - 6, G2048_CELL - 6, bg);
            M5.Display.drawRect(x + 3, y + 3, G2048_CELL - 6, G2048_CELL - 6, TFT_BLACK);

            if (v != 0) {
                String text = String(v);
                M5.Display.setFont(tileFont(v));
                M5.Display.setTextColor(tileTextColor(v), bg);

                int tw = M5.Display.textWidth(text);
                int th = M5.Display.fontHeight();
                int tx = x + (G2048_CELL - tw) / 2;
                int ty = y + (G2048_CELL - th) / 2;
                M5.Display.drawString(text, tx, ty);
            }
        }
    }
}

void drawHUD2048(const char* status) {
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // Scores
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.drawString("Score: " + String(g2048_score) +
                          "   Best: " + String(g2048_best),
                          20, 540);

    // Status
    String line;
    if (status != nullptr) {
        line = String(status);
    } else if (g2048_gameOver) {
        line = "Game Over!";
    } else if (g2048_won) {
        line = "You reached 2048!";
    } else {
        line = "Have fun!";
    }
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString(line, 20, 580);

    // Instructions
    // M5.Display.setFont(&fonts::FreeSans9pt7b);
    // M5.Display.drawString("Swipe on board to move", 20, 740);

    // Version
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString("2048 v1.0", 20, 760);
}

void redrawGame2048Quality() {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    beginDisplay();
    clearScreen();
    drawHeader2048();
    drawBoard2048();
    drawHUD2048();
    endDisplay();
}

void redrawGame2048(const char* status) {
    static int fastRefreshCount = 0;

    if (fastRefreshCount >= 10) {
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
        fastRefreshCount = 0;
    } else {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        fastRefreshCount++;
    }

    beginDisplay();
    clearScreen();
    drawHeader2048();
    drawBoard2048();
    drawHUD2048(status);
    endDisplay();
}

// ============================================================
// Touch swipe detection
// ============================================================

static bool touchActive = false;
static int touchStartX = 0;
static int touchStartY = 0;
static unsigned long touchStartTime = 0;
static bool swipeHandled = false;

static const int SWIPE_MIN_DIST = 40;
static const unsigned long SWIPE_MAX_MS = 800;

void handleSwipe() {
    if (M5.Touch.getCount() > 0) {
        auto detail = M5.Touch.getDetail(0);
        if (detail.isPressed()) {
            int mx, my;
            mapTouchToDisplay(detail.x, detail.y, mx, my);

            if (!touchActive) {
                touchActive = true;
                touchStartX = mx;
                touchStartY = my;
                touchStartTime = millis();
                swipeHandled = false;
            }
        } else if (touchActive) {
            if (!swipeHandled) {
                int mx, my;
                mapTouchToDisplay(detail.x, detail.y, mx, my);
                int dx = mx - touchStartX;
                int dy = my - touchStartY;
                unsigned long dt = millis() - touchStartTime;

                if (dt < SWIPE_MAX_MS) {
                    int adx = abs(dx);
                    int ady = abs(dy);

                    if (adx > SWIPE_MIN_DIST || ady > SWIPE_MIN_DIST) {
                        int dir = -1;
                        if (adx > ady) {
                            dir = (dx > 0) ? 3 : 2;
                        } else {
                            dir = (dy > 0) ? 1 : 0;
                        }

                        if (dir >= 0) {
                            if (tryMove(dir)) {
                                redrawGame2048();
                            }
                            swipeHandled = true;
                        }
                    } else if (hitInfoButton(mx, my)) {
                        // It's a tap (not a swipe) on the info button
                        static const char* const game2048Help[] = {
                            "Swipe on the board = Move tiles",
                            "",
                            "Hold BtnA = Launcher",
                            "Hold BtnB = New game",
                            "Hold BtnA + BtnB = Sleep / Wake",
                        };
                        showInfoModalAndWait("2048 v1.0", game2048Help, 5);

                        // Redraw the 2048 board
                        redrawGame2048Quality();
                        swipeHandled = true;
                    } else if (hitBackButton(mx, my)) {
                        enterLauncher();
                        swipeHandled = true;
                    }
                }
            }
            touchActive = false;
            resetTouch();
        }
    } else {
        touchActive = false;
    }
}

// ============================================================
// Public API
// ============================================================

void initGame2048() {
    loadBestScore();

    if (loadProgress()) {
        Serial.println("2048: Resumed saved game");
        g2048_initialized = true;
        g2048_lastActivity = millis();
        return;
    }

    clearBoard();
    g2048_score = 0;
    g2048_gameOver = false;
    g2048_won = false;
    g2048_initialized = true;
    g2048_lastActivity = millis();
    spawnTile();
    spawnTile();
    saveProgress();
}

void newGame2048() {
    initGame2048();
    redrawGame2048Quality();
}

void restartGame2048() {
    saveGameDelete(G2048_SAVE_PATH);
    initGame2048();
    redrawGame2048("New game!");
    delay(400);
    redrawGame2048Quality();
}

void game2048Loop() {
    if (M5.BtnA.wasHold()) {
        if (confirmDialog("Quit?", "Return to the launcher?", "Quit", "Stay")) {
            enterLauncher();
        } else {
            redrawGame2048Quality();
        }
        return;
    }

    if (M5.BtnB.wasHold()) {
        if (confirmDialog("Restart?", "Start a new game?", "Restart", "Cancel")) {
            restartGame2048();
        } else {
            redrawGame2048Quality();
        }
        return;
    }

    handleSwipe();
}
