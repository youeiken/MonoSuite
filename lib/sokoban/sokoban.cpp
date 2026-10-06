#include "sokoban.h"
#include "launcher.h"

#include "appmode.h"
#include "appstate.h"
#include "display.h"
#include "storage.h"
#include "touch.h"
#include "savegame.h"
#include <LittleFS.h>
#include <SD.h>

static void drawNextButton();
static bool hitNextButton(int x, int y);
static bool isCornerDeadlock(int r, int c);
static void clearGrids();
static bool detectDeadlock();

// ============================================================
// Game State
// ============================================================

bool sk_initialized = false;
bool sk_won = false;
bool sk_deadlocked = false;
int  sk_levelIndex = 0;
int  sk_moveCount = 0;
int  sk_pushCount = 0;
unsigned long sk_lastActivity = 0;

int sk_cell = 40;

static int sk_cols = 0;
static int sk_rows = 0;

static int sk_base[SK_MAX_ROWS][SK_MAX_COLS];
static int sk_ent[SK_MAX_ROWS][SK_MAX_COLS];

// The height available for the board, below the header and above the HUD.
// Header is 70px; HUD stats begin at 620. Leave a small margin.
static const int SK_AVAIL_H = 600 - SK_BOARD_TOP;   // = 530

// ============================================================
// Level storage
// ============================================================

struct SkLevel {
    char grid[SK_MAX_ROWS][SK_MAX_COLS + 1];
    int  rows;
    int  cols;
    char title[48];
};

static const int SK_MAX_LEVELS_IN_MEMORY = 128;
static SkLevel* sk_levels = nullptr;
static int      sk_levelCount = 0;

// ============================================================
// Undo — single-slot. At most one move can be undone, and only
// immediately after a move. Making a new move overwrites the slot;
// undoing consumes it.
// ============================================================

struct SkMove {
    int r, c;      // player position before the move
    int dr, dc;    // direction of the move
    bool pushed;   // did it push a box?
};

static SkMove sk_lastMove;
static bool   sk_hasLastMove = false;

// ============================================================
// Save / Load progress
// ============================================================

static const char* SK_SAVE_PATH = "/sokoban_save.txt";

static void saveProgress() {
    if (sk_levelCount == 0) return;
    if (sk_won) {
        saveGameDelete(SK_SAVE_PATH);
        return;
    }

    String s = "";
    s += "level=" + String(sk_levelIndex) + "\n";
    s += "moves=" + String(sk_moveCount) + "\n";
    s += "pushes=" + String(sk_pushCount) + "\n";
    s += "rows=" + String(sk_rows) + "\n";
    s += "cols=" + String(sk_cols) + "\n";
    s += "board=\n";
    for (int r = 0; r < sk_rows; r++) {
        for (int c = 0; c < sk_cols; c++) {
            s += String(sk_base[r][c]) + "," + String(sk_ent[r][c]) + ";";
        }
        s += "\n";
    }
    saveGameWrite(SK_SAVE_PATH, s);
}

static bool loadProgress() {
    if (!saveGameExists(SK_SAVE_PATH)) return false;
    String s = saveGameRead(SK_SAVE_PATH);
    if (s.length() == 0) return false;

    int savedLevel = -1;
    int savedMoves = 0;
    int savedPushes = 0;
    int savedRows = 0;
    int savedCols = 0;
    int lineStart = 0;
    bool inBoard = false;
    int boardRow = 0;

    clearGrids();

    while (lineStart < s.length()) {
        int lineEnd = s.indexOf('\n', lineStart);
        if (lineEnd < 0) lineEnd = s.length();
        String line = s.substring(lineStart, lineEnd);
        line.trim();

        if (inBoard) {
            if (line.length() == 0) {
                inBoard = false;
            } else if (boardRow < SK_MAX_ROWS && boardRow < savedRows) {
                int c = 0;
                int start = 0;
                while (c < savedCols && start < (int)line.length()) {
                    int semi = line.indexOf(';', start);
                    if (semi < 0) break;
                    String cell = line.substring(start, semi);
                    int comma = cell.indexOf(',');
                    if (comma > 0) {
                        sk_base[boardRow][c] = cell.substring(0, comma).toInt();
                        sk_ent[boardRow][c] = cell.substring(comma + 1).toInt();
                    }
                    c++;
                    start = semi + 1;
                }
                boardRow++;
            }
        } else {
            if (line.startsWith("level="))  savedLevel = line.substring(6).toInt();
            else if (line.startsWith("moves="))  savedMoves = line.substring(6).toInt();
            else if (line.startsWith("pushes=")) savedPushes = line.substring(7).toInt();
            else if (line.startsWith("rows="))   savedRows = line.substring(5).toInt();
            else if (line.startsWith("cols="))   savedCols = line.substring(5).toInt();
            else if (line.startsWith("board="))  inBoard = true;
        }
        lineStart = lineEnd + 1;
    }

    if (savedLevel < 0 || savedLevel >= sk_levelCount) return false;
    if (savedRows <= 0 || savedCols <= 0) return false;

    sk_levelIndex = savedLevel;
    sk_rows = savedRows;
    sk_cols = savedCols;
    sk_moveCount = savedMoves;
    sk_pushCount = savedPushes;
    sk_won = false;
    sk_deadlocked = detectDeadlock();

    // A freshly-loaded game has no undoable move.
    sk_hasLastMove = false;

    // Re-derive sk_cell from the loaded dimensions, respecting the header
    int availW = 480;
    int availH = SK_AVAIL_H;
    int cellW = (sk_cols > 0) ? availW / sk_cols : 40;
    int cellH = (sk_rows > 0) ? availH / sk_rows : 40;
    sk_cell = (cellW < cellH) ? cellW : cellH;
    if (sk_cell > 40) sk_cell = 40;
    if (sk_cell < 8)  sk_cell = 8;

    Serial.printf("[Sokoban] Resumed level %d (moves=%d)\n", savedLevel, savedMoves);
    return true;
}

// ============================================================
// XSB loader
// ============================================================

static bool isLevelLine(const String& line) {
    if (line.length() == 0) return false;
    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];
        if (c != '#' && c != '-' && c != ' ' && c != '$' &&
            c != '.' && c != '@' && c != '*' && c != '+') {
            return false;
        }
    }
    return true;
}

static bool isMetadataLine(const String& line) {
    return line.startsWith("Title:") ||
           line.startsWith("Comment:") ||
           line.startsWith("Author:") ||
           line.startsWith("Date:") ||
           line.startsWith("Mail:") ||
           line.startsWith("Url:") ||
           line.startsWith("Firstdate:") ||
           line.startsWith("Lastdate:") ||
           line.startsWith("Collection:") ||
           line.startsWith("Previously published:");
}

static void saveCurrentLevel(SkLevel& cur, bool& inLevel) {
    if (inLevel && cur.rows > 0 &&
        sk_levels != nullptr && sk_levelCount < SK_MAX_LEVELS_IN_MEMORY) {
        sk_levels[sk_levelCount++] = cur;
    }
    cur.rows = 0;
    cur.cols = 0;
    cur.title[0] = '\0';
    inLevel = false;
}

static void loadXsbFiles() {
    sk_levelCount = 0;

    if (!isSdMounted()) {
        Serial.println("[Sokoban] SD not mounted");
        return;
    }
    if (!SD.exists("/sokoban")) {
        Serial.println("[Sokoban] /sokoban directory not found");
        return;
    }

    File dir = SD.open("/sokoban");
    if (!dir || !dir.isDirectory()) {
        Serial.println("[Sokoban] Failed to open /sokoban");
        return;
    }

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;

        String name = entry.name();
        String lower = name;
        lower.toLowerCase();
        if (!lower.endsWith(".xsb")) {
            entry.close();
            continue;
        }

        Serial.printf("[Sokoban] Loading: %s\n", name.c_str());

        SkLevel current;
        current.rows = 0;
        current.cols = 0;
        current.title[0] = '\0';
        bool inLevel = false;

        while (entry.available()) {
            String line = entry.readStringUntil('\n');
            if (line.endsWith("\r")) line = line.substring(0, line.length() - 1);

            if (isMetadataLine(line)) {
                if (line.startsWith("Title:") && inLevel) {
                    String t = line.substring(6);
                    t.trim();
                    strncpy(current.title, t.c_str(), sizeof(current.title) - 1);
                    current.title[sizeof(current.title) - 1] = '\0';
                }
                continue;
            }

            if (line.length() == 0) {
                saveCurrentLevel(current, inLevel);
                continue;
            }

            if (!isLevelLine(line)) {
                continue;
            }

            inLevel = true;
            if (current.rows < SK_MAX_ROWS) {
                int len = line.length();
                if (len > SK_MAX_COLS) len = SK_MAX_COLS;
                for (int i = 0; i < len; i++) {
                    current.grid[current.rows][i] = line[i];
                }
                for (int i = len; i < SK_MAX_COLS; i++) {
                    current.grid[current.rows][i] = ' ';
                }
                current.grid[current.rows][SK_MAX_COLS] = '\0';
                if (len > current.cols) current.cols = len;
                current.rows++;
            }
        }

        saveCurrentLevel(current, inLevel);
        entry.close();
    }

    dir.close();
    Serial.printf("[Sokoban] Loaded %d levels\n", sk_levelCount);
}

// ============================================================
// Level loading
// ============================================================

static void clearGrids() {
    for (int r = 0; r < SK_MAX_ROWS; r++) {
        for (int c = 0; c < SK_MAX_COLS; c++) {
            sk_base[r][c] = SK_FLOOR;
            sk_ent[r][c] = 0;
        }
    }
    sk_cols = 0;
    sk_rows = 0;
}

static void loadLevelFromStruct(const SkLevel& lvl) {
    clearGrids();

    sk_rows = lvl.rows;
    sk_cols = lvl.cols;

    for (int r = 0; r < sk_rows; r++) {
        for (int c = 0; c < sk_cols; c++) {
            char ch = lvl.grid[r][c];
            switch (ch) {
                case '#': sk_base[r][c] = SK_WALL; break;
                case '.': sk_base[r][c] = SK_GOAL; break;
                case '$': sk_ent[r][c] |= SK_BOX; break;
                case '*': sk_base[r][c] = SK_GOAL; sk_ent[r][c] |= SK_BOX; break;
                case '@': sk_ent[r][c] |= SK_PLAYER; break;
                case '+': sk_base[r][c] = SK_GOAL; sk_ent[r][c] |= SK_PLAYER; break;
                default: break;
            }
        }
    }

    // Auto-scale cell size to fit inside 480 wide × SK_AVAIL_H tall
    int availW = 480;
    int availH = SK_AVAIL_H;
    int cellW = (sk_cols > 0) ? availW / sk_cols : 40;
    int cellH = (sk_rows > 0) ? availH / sk_rows : 40;
    sk_cell = (cellW < cellH) ? cellW : cellH;
    if (sk_cell > 40) sk_cell = 40;
    if (sk_cell < 8)  sk_cell = 8;
}

static void loadLevel(int index) {
    if (sk_levelCount == 0) return;
    if (index < 0 || index >= sk_levelCount) index = 0;
    sk_levelIndex = index;
    loadLevelFromStruct(sk_levels[index]);

    sk_moveCount = 0;
    sk_pushCount = 0;
    sk_won = false;
    sk_deadlocked = false;
    sk_hasLastMove = false;
    saveGameDelete(SK_SAVE_PATH);
}

// ============================================================
// Core logic
// ============================================================

static bool inBounds(int r, int c) {
    return r >= 0 && r < sk_rows && c >= 0 && c < sk_cols;
}

static bool isWall(int r, int c) {
    return !inBounds(r, c) || sk_base[r][c] == SK_WALL;
}

static bool hasBox(int r, int c) {
    return inBounds(r, c) && (sk_ent[r][c] & SK_BOX);
}

static bool findPlayer(int& r, int& c) {
    for (int rr = 0; rr < sk_rows; rr++) {
        for (int cc = 0; cc < sk_cols; cc++) {
            if (sk_ent[rr][cc] & SK_PLAYER) {
                r = rr; c = cc;
                return true;
            }
        }
    }
    return false;
}

static bool checkWin() {
    for (int r = 0; r < sk_rows; r++) {
        for (int c = 0; c < sk_cols; c++) {
            if (sk_ent[r][c] & SK_BOX) {
                if (sk_base[r][c] != SK_GOAL) return false;
            }
        }
    }
    return true;
}

static bool isCornerDeadlock(int r, int c) {
    if (sk_base[r][c] == SK_GOAL) return false;
    bool up    = isWall(r - 1, c);
    bool down  = isWall(r + 1, c);
    bool left  = isWall(r, c - 1);
    bool right = isWall(r, c + 1);
    return (up && left) || (up && right) || (down && left) || (down && right);
}

static bool isBlockDeadlock() {
    for (int r = 0; r + 1 < sk_rows; r++) {
        for (int c = 0; c + 1 < sk_cols; c++) {
            if (!(sk_ent[r][c]     & SK_BOX)) continue;
            if (!(sk_ent[r+1][c]   & SK_BOX)) continue;
            if (!(sk_ent[r][c+1]   & SK_BOX)) continue;
            if (!(sk_ent[r+1][c+1] & SK_BOX)) continue;
            if (sk_base[r][c]     != SK_GOAL ||
                sk_base[r+1][c]   != SK_GOAL ||
                sk_base[r][c+1]   != SK_GOAL ||
                sk_base[r+1][c+1] != SK_GOAL) {
                return true;
            }
        }
    }
    return false;
}

static bool detectDeadlock() {
    for (int r = 0; r < sk_rows; r++) {
        for (int c = 0; c < sk_cols; c++) {
            if ((sk_ent[r][c] & SK_BOX) && isCornerDeadlock(r, c)) {
                return true;
            }
        }
    }
    if (isBlockDeadlock()) return true;
    return false;
}

static bool tryMove(int dr, int dc) {
    if (sk_won || sk_deadlocked) return false;

    int pr, pc;
    if (!findPlayer(pr, pc)) return false;

    int nr = pr + dr;
    int nc = pc + dc;

    if (isWall(nr, nc)) return false;

    bool pushed = false;

    if (hasBox(nr, nc)) {
        int br = nr + dr;
        int bc = nc + dc;
        if (isWall(br, bc) || hasBox(br, bc)) return false;

        sk_ent[nr][nc] &= ~SK_BOX;
        sk_ent[br][bc] |= SK_BOX;
        sk_pushCount++;
        pushed = true;
    }

    sk_ent[pr][pc] &= ~SK_PLAYER;
    sk_ent[nr][nc] |= SK_PLAYER;
    sk_moveCount++;

    // Record the move into the single-slot undo buffer.
    sk_lastMove = { pr, pc, dr, dc, pushed };
    sk_hasLastMove = true;

    if (checkWin()) {
        sk_won = true;
        saveGameDelete(SK_SAVE_PATH);
    } else {
        sk_deadlocked = detectDeadlock();
        saveProgress();
    }
    return true;
}

static void undoMove() {
    if (!sk_hasLastMove) return;

    SkMove m = sk_lastMove;

    int pr = m.r + m.dr;
    int pc = m.c + m.dc;

    if (m.pushed) {
        int boxR = pr + m.dr;
        int boxC = pc + m.dc;
        sk_ent[boxR][boxC] &= ~SK_BOX;
        sk_ent[pr][pc] |= SK_BOX;
        if (sk_pushCount > 0) sk_pushCount--;
    }

    sk_ent[pr][pc] &= ~SK_PLAYER;
    sk_ent[m.r][m.c] |= SK_PLAYER;
    if (sk_moveCount > 0) sk_moveCount--;

    sk_won = false;
    sk_deadlocked = false;

    sk_hasLastMove = false;
    saveProgress();
}

// ============================================================
// Drawing
// ============================================================

static void drawHeaderSokoban() {
    drawHeader("", true);
}

static void drawCell(int r, int c) {
    int x = SK_BOARD_LEFT + c * sk_cell;
    int y = SK_BOARD_TOP + r * sk_cell;

    int base = sk_base[r][c];
    int ent = sk_ent[r][c];

    if (base == SK_WALL) {
        M5.Display.fillRect(x, y, sk_cell, sk_cell, TFT_BLACK);
        return;
    }

    uint16_t floorColor = (base == SK_GOAL) ? 0xCE79 : TFT_WHITE;
    M5.Display.fillRect(x, y, sk_cell, sk_cell, floorColor);

    if (sk_cell >= 16) {
        M5.Display.drawRect(x, y, sk_cell, sk_cell, 0x8410);
    }

    if (base == SK_GOAL) {
        int pad = sk_cell / 4;
        if (pad < 3) pad = 3;
        int thick = sk_cell / 8;
        if (thick < 2) thick = 2;
        M5.Display.fillRect(x + pad, y + pad,
                            sk_cell - 2 * pad, sk_cell - 2 * pad,
                            TFT_BLACK);
        M5.Display.fillRect(x + pad + thick, y + pad + thick,
                            sk_cell - 2 * pad - 2 * thick,
                            sk_cell - 2 * pad - 2 * thick,
                            floorColor);
    }

    if (ent & SK_BOX) {
        int pad = sk_cell / 6;
        if (pad < 2) pad = 2;
        int bx = x + pad;
        int by = y + pad;
        int bw = sk_cell - 2 * pad;
        int bh = sk_cell - 2 * pad;

        M5.Display.fillRect(bx, by, bw, bh, TFT_BLACK);

        int inner = pad + (sk_cell / 12 > 1 ? sk_cell / 12 : 1);
        int ix = x + inner;
        int iy = y + inner;
        int iw = sk_cell - 2 * inner;
        int ih = sk_cell - 2 * inner;
        uint16_t innerColor = (base == SK_GOAL) ? 0x8410 : TFT_WHITE;
        M5.Display.fillRect(ix, iy, iw, ih, innerColor);

        if (sk_cell >= 12) {
            M5.Display.drawLine(ix, iy, ix + iw - 1, iy + ih - 1, TFT_BLACK);
            M5.Display.drawLine(ix + iw - 1, iy, ix, iy + ih - 1, TFT_BLACK);
            if (sk_cell >= 20) {
                M5.Display.drawLine(ix, iy + 1, ix + iw - 1, iy + ih, TFT_BLACK);
                M5.Display.drawLine(ix + 1, iy, ix + iw, iy + ih - 1, TFT_BLACK);
                M5.Display.drawLine(ix, iy + ih - 1, ix + iw - 1, iy, TFT_BLACK);
                M5.Display.drawLine(ix + 1, iy + ih - 1, ix + iw - 1, iy + 1, TFT_BLACK);
            }
        }

        if (sk_deadlocked && isCornerDeadlock(r, c)) {
            int pad = sk_cell / 4;
            M5.Display.drawLine(x + pad, y + pad,
                                x + sk_cell - pad, y + sk_cell - pad,
                                TFT_WHITE);
            M5.Display.drawLine(x + sk_cell - pad, y + pad,
                                x + pad, y + sk_cell - pad,
                                TFT_WHITE);
        }
    }

    if (ent & SK_PLAYER) {
        int r2 = sk_cell / 2 - sk_cell / 8;
        if (r2 < 2) r2 = 2;
        M5.Display.fillCircle(x + sk_cell / 2, y + sk_cell / 2, r2, TFT_BLACK);
    }
}

static void drawBoardSokoban() {
    for (int r = 0; r < sk_rows; r++) {
        for (int c = 0; c < sk_cols; c++) {
            drawCell(r, c);
        }
    }
}

static void drawHUDSokoban(const char* status = nullptr) {
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // Level indicator and move/push counts
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.drawString("Level " + String(sk_levelIndex + 1) +
                          "/" + String(sk_levelCount),
                          310, 620);
    M5.Display.drawString("Moves: " + String(sk_moveCount) +
                          "   Pushes: " + String(sk_pushCount),
                          20, 620);

    // Status line
    String line;
    if (status != nullptr) {
        line = String(status);
    } else if (sk_won) {
        line = "Level complete!";
    } else if (sk_deadlocked) {
        line = "Warning: box stuck!";
    } else if (sk_levelCount == 0) {
        line = "No levels - put .xsb in /sokoban";
    } else {
        line = "Have fun!";
    }
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString(line, 20, 660);

    // Version
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString("SOKOBAN v1.0", 20, 760);
}

void redrawSokobanQuality() {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    beginDisplay();
    clearScreen();
    drawHeaderSokoban();
    drawBoardSokoban();
    drawHUDSokoban();
    if (sk_won) drawNextButton();
    endDisplay();
}

void redrawSokoban(const char* status) {
    static int fastRefreshCount = 0;
    if (fastRefreshCount >= 1000) {
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
        fastRefreshCount = 0;
    } else {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        fastRefreshCount++;
    }

    beginDisplay();
    clearScreen();
    drawHeaderSokoban();
    drawBoardSokoban();
    drawHUDSokoban(status);
    if (sk_won) drawNextButton();
    endDisplay();
}

static void getNextButtonBounds(int& bx, int& by, int& bw, int& bh) {
    bw = 240;
    bh = 80;
    bx = (480 - bw) / 2;
    by = (800 - bh) / 2;
}

static void drawNextButton() {
    int bx, by, bw, bh;
    getNextButtonBounds(bx, by, bw, bh);

    M5.Display.fillRoundRect(bx, by, bw, bh, 12, TFT_BLACK);
    M5.Display.drawRoundRect(bx, by, bw, bh, 12, TFT_WHITE);

    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    const char* label = "Next Level";
    int tw = M5.Display.textWidth(label);
    int th = M5.Display.fontHeight();
    M5.Display.drawString(label, bx + (bw - tw) / 2, by + (bh - th) / 2);
}

static bool hitNextButton(int x, int y) {
    int bx, by, bw, bh;
    getNextButtonBounds(bx, by, bw, bh);
    return x >= bx && x <= bx + bw && y >= by && y <= by + bh;
}

// ============================================================
// Swipe handling
// ============================================================

static bool          skTouchActive = false;
static int           skTouchStartX = 0;
static int           skTouchStartY = 0;
static unsigned long skTouchStartTime = 0;
static bool          skSwipeHandled = false;

static const int           SK_SWIPE_MIN = 30;
static const unsigned long SK_SWIPE_MAX_MS = 900;

static void doDirection(int dir) {
    bool moved = false;
    switch (dir) {
        case 0: moved = tryMove(-1,  0); break;
        case 1: moved = tryMove( 1,  0); break;
        case 2: moved = tryMove( 0, -1); break;
        case 3: moved = tryMove( 0,  1); break;
    }
    if (moved) {
        redrawSokoban();
        sk_lastActivity = millis();
    }
}

static void handleSokobanSwipe() {
    if (M5.Touch.getCount() > 0) {
        auto detail = M5.Touch.getDetail(0);
        if (detail.isPressed()) {
            int mx, my;
            mapTouchToDisplay(detail.x, detail.y, mx, my);
            if (!skTouchActive) {
                skTouchActive = true;
                skTouchStartX = mx;
                skTouchStartY = my;
                skTouchStartTime = millis();
                skSwipeHandled = false;
            }
        } else if (skTouchActive) {
            if (!skSwipeHandled) {
                int mx, my;
                mapTouchToDisplay(detail.x, detail.y, mx, my);
                int dx = mx - skTouchStartX;
                int dy = my - skTouchStartY;
                unsigned long dt = millis() - skTouchStartTime;
                if (dt < SK_SWIPE_MAX_MS) {
                    int adx = abs(dx);
                    int ady = abs(dy);
                    if (adx > SK_SWIPE_MIN || ady > SK_SWIPE_MIN) {
                        int dir;
                        if (adx > ady) dir = (dx > 0) ? 3 : 2;
                        else           dir = (dy > 0) ? 1 : 0;
                        doDirection(dir);
                        skSwipeHandled = true;
                    }
                }
            }
            skTouchActive = false;
            resetTouch();
        }
    } else {
        skTouchActive = false;
    }
}

// ============================================================
// Public API
// ============================================================

void initSokoban() {
    if (sk_levels == nullptr) {
        sk_levels = (SkLevel*)heap_caps_malloc(
            SK_MAX_LEVELS_IN_MEMORY * sizeof(SkLevel),
            MALLOC_CAP_SPIRAM
        );
        if (sk_levels == nullptr) {
            Serial.println("[Sokoban] PSRAM allocation failed, falling back to SRAM");
            sk_levels = (SkLevel*)malloc(SK_MAX_LEVELS_IN_MEMORY * sizeof(SkLevel));
        }
        if (sk_levels == nullptr) {
            Serial.println("[Sokoban] Level buffer allocation failed entirely");
            sk_levelCount = 0;
            return;
        }
        Serial.printf("[Sokoban] Level buffer: %d bytes\n",
                      (int)(SK_MAX_LEVELS_IN_MEMORY * sizeof(SkLevel)));
    }

    if (sk_levelCount == 0) {
        loadXsbFiles();
    }

    if (loadProgress()) {
        sk_initialized = true;
        sk_lastActivity = millis();
        return;
    }

    if (sk_levelCount > 0) {
        sk_levelIndex = 0;
        loadLevel(0);
    } else {
        clearGrids();
        sk_rows = 0;
        sk_cols = 0;
        sk_cell = 40;
    }

    sk_initialized = true;
    sk_lastActivity = millis();
}

void newSokobanGame() {
    sk_levelIndex = 0;
    initSokoban();
    redrawSokobanQuality();
}

void restartSokobanGame() {
    if (sk_levelCount == 0) return;
    loadLevel(sk_levelIndex);
    redrawSokoban("Restart!");
    delay(400);
    redrawSokobanQuality();
}

void loadNextSokobanLevel() {
    if (sk_levelCount == 0) return;
    sk_levelIndex = (sk_levelIndex + 1) % sk_levelCount;
    loadLevel(sk_levelIndex);
    String msg = "Level " + String(sk_levelIndex + 1) + "!";
    redrawSokoban(msg.c_str());
    delay(400);
    redrawSokobanQuality();
}

void sokobanLoop() {
    if (M5.BtnA.wasHold()) {
        if (confirmDialog("Quit?", "Return to the launcher?", "Quit", "Stay")) {
            enterLauncher();
        } else {
            redrawSokobanQuality();
        }
        return;
    }

    if (M5.BtnB.wasHold()) {
        if (confirmDialog("Restart?", "Restart this level?", "Restart", "Cancel")) {
            restartSokobanGame();
        } else {
            redrawSokobanQuality();
        }
        return;
    }

    if (M5.BtnB.wasClicked()) {
        if (sk_won) return;
        if (!sk_hasLastMove) {
            redrawSokoban("Nothing to undo");
            delay(300);
            redrawSokoban();
            return;
        }
        undoMove();
        redrawSokoban("Undo");
        delay(150);
        redrawSokoban();
        return;
    }

    // Single tap handler: back button first, then info button, then Next Level
    int x, y;
    if (getTouch(x, y)) {
        if (hitBackButton(x, y)) {
            enterLauncher();
            resetTouch();
            return;
        }

        if (hitInfoButton(x, y)) {
            static const char* const sokobanHelp[] = {
                "Swipe = Move the player",
                "BtnB (short) = Undo (once per move)",
                "",
                "Hold BtnA = Launcher",
                "Hold BtnB = Restart level",
                "Hold BtnA + BtnB = Sleep / Wake",
            };
            showInfoModalAndWait("SOKOBAN v1.0", sokobanHelp, 6);

            redrawSokobanQuality();
            resetTouch();
            return;
        }

        if (sk_won && hitNextButton(x, y)) {
            loadNextSokobanLevel();
            resetTouch();
            return;
        }

        resetTouch();
    }

    handleSokobanSwipe();
}
