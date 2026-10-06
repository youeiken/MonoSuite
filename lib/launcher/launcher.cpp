#include "launcher.h"

#include "appmode.h"
#include "appstate.h"
#include "display.h"
#include "storage.h"
#include "touch.h"
#include "time_sync.h"
#include "wifi_setup.h"

// Brightness levels for cycling
const int BRIGHTNESS_LEVELS[] = {0, 32, 64, 96, 128, 192, 255};
const int BRIGHTNESS_COUNT = 7;

// ============================================================
// Header clock — centered between the back and info buttons
// ============================================================
void drawHeaderClock() {
    String t = displayTimeFromRTC();

    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    int tw = M5.Display.textWidth(t);
    int th = M5.Display.fontHeight();

    // If you want it sitting between the battery label and the info button instead of dead-center
    // int rightEdge = 330;   // just left of the battery percentage label
    // int x = rightEdge - tw;

    // Center horizontally on the 480-wide portrait display.
    // Vertically align with the button centers (INFO_BTN_Y / BACK_BTN_Y == 45).
    int x = (480 - tw) / 2;
    int y = 48 - th / 2;

    M5.Display.drawString(t, x, y);
}

// ============================================================
// Battery indicator — drawn in the top-right corner
// ============================================================
void drawBattery() {
    int batteryLevel = M5.Power.getBatteryLevel();
    if (batteryLevel < 0) batteryLevel = 0;
    if (batteryLevel > 100) batteryLevel = 100;

    // Battery sits in the header row, vertically centered on the title
    int xPos = 370, yPos = 35;

    // Battery body
    M5.Display.drawRect(xPos, yPos, 40, 20, TFT_BLACK);
    // Battery cap
    M5.Display.drawRect(xPos + 40, yPos + 5, 4, 10, TFT_BLACK);

    int fillWidth = (batteryLevel / 100.0) * 36;
    if (fillWidth > 36) fillWidth = 36;

    uint16_t color;
    if (batteryLevel > 50) color = TFT_GREEN;
    else if (batteryLevel > 20) color = TFT_ORANGE;
    else color = TFT_RED;

    if (fillWidth > 0) {
        M5.Display.fillRect(xPos + 2, yPos + 2, fillWidth, 16, color);
    }

    // Percentage label to the left of the battery
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    String label = String(batteryLevel) + "%";
    int tw = M5.Display.textWidth(label);
    M5.Display.drawString(label, xPos - tw - 6, yPos + 2);
}

// ============================================================
// Back button — mirrors the info button on the left side
// ============================================================
static const int BACK_BTN_X = 35;    // center x
static const int BACK_BTN_Y = 45;    // center y (same baseline as info)
static const int BACK_BTN_R = 15;    // radius (same as info)

static void drawBackGlyph(int cx, int cy, int r) {
    // Arrow "<" with a thicker shaft. Strokes are offset along a small
    // 2D grid so both the shallow and steep parts of the angle fill in.
    int tipX = cx - (r * 45) / 100;
    int armX = cx + (r * 15) / 100;
    int topY = cy - (r * 90) / 100;
    int botY = cy + (r * 90) / 100;

    // 3 x 2 = 6 strokes → about a 3 px thick shaft.
    // Bump the outer bound if you want it chunkier.
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            // Only use offsets that lie within a disk of radius ~1
            // so corners aren't double-drawn.
            if (dx * dx + dy * dy > 2) continue;
            M5.Display.drawLine(armX + dx, topY + dy, tipX + dx, cy + dy, TFT_BLACK);
            M5.Display.drawLine(tipX + dx, cy + dy, armX + dx, botY + dy, TFT_BLACK);
        }
    }
}

void drawBackButton() {
    // M5.Display.fillCircle(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_R, TFT_BLACK);
    // M5.Display.drawCircle(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_R, TFT_BLACK);
    drawBackGlyph(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_R);
}

bool hitBackButton(int x, int y) {
    int dx = x - BACK_BTN_X;
    int dy = y - BACK_BTN_Y;
    return (dx * dx + dy * dy) <= (BACK_BTN_R * BACK_BTN_R);
}

// ============================================================
// Info button — a small black circle with a white "i"
// ============================================================
static const int INFO_BTN_X = 440;   // center x
static const int INFO_BTN_Y = 45;    // center y
static const int INFO_BTN_R = 15;    // radius

static void drawInfoButton() {
    M5.Display.fillCircle(INFO_BTN_X, INFO_BTN_Y, INFO_BTN_R, TFT_BLACK);
    M5.Display.drawCircle(INFO_BTN_X, INFO_BTN_Y, INFO_BTN_R, TFT_BLACK);

    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    const char* glyph = "i";
    int tw = M5.Display.textWidth(glyph);
    int th = M5.Display.fontHeight();
    M5.Display.drawString(glyph,
                          INFO_BTN_X - tw / 2,
                          INFO_BTN_Y - th / 2 + 2);
}

bool hitInfoButton(int x, int y) {
    int dx = x - INFO_BTN_X;
    int dy = y - INFO_BTN_Y;
    return (dx * dx + dy * dy) <= (INFO_BTN_R * INFO_BTN_R);
}

// ============================================================
// Shared header — used by launcher and all games
// ============================================================
static void drawHeaderInternal(const String& title,
                               const lgfx::IFont* font,
                               bool withBack) {
    M5.Display.setFont(font);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // Title x depends on whether a back button occupies the left side.
    int titleX = withBack ? 80 : 20;

    M5.Display.drawString(title, titleX, 33);

    if (withBack) drawBackButton();
    drawInfoButton();
    drawBattery();
    drawHeaderClock();
}

void drawHeader(const char* title, bool withBack) {
    drawHeaderInternal(String(title), &fonts::FreeSansBold18pt7b, withBack);
}

void drawHeader(const String& title, bool withBack) {
    drawHeaderInternal(title, &fonts::FreeSansBold18pt7b, withBack);
}

void drawHeader(const String& title, const lgfx::IFont* font, bool withBack) {
    drawHeaderInternal(title, font, withBack);
}

// ============================================================
// WiFi icon (unused, kept for future use)
// ============================================================
void drawWifiIcon() {
    bool connected = isWifiConnected();

    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    const char* label = connected ? "WiFi on" : "WiFi off";
    int tw = M5.Display.textWidth(label);
    int x = 330 - tw;
    int y = 32;

    M5.Display.drawString(label, x, y);
}

// ============================================================
// Button helper
// ============================================================
void drawButton(int x, int y, int w, int h, const char* title, const char* sub) {
    M5.Display.fillRoundRect(x, y, w, h, 12, TFT_BLACK);
    M5.Display.setFont(&fonts::FreeSans18pt7b);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString(title, x + 30, y + 25);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString(sub, x + 30, y + 60);
}

// ============================================================
// Launcher screen
// ============================================================
void drawLauncher() {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    beginDisplay();
    clearScreen();

    drawHeader("");
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextColor(0x8410, TFT_WHITE);
    // M5.Display.drawString("Launcher", 135, 40);

    drawButton(20, 80, 440, 100, "REVERSI",     "Human vs AI");
    drawButton(20, 195, 440, 100, "2048",        "Merge the tiles");
    drawButton(20, 310, 440, 100, "MINESWEEPER", "Clear the field");
    drawButton(20, 425, 440, 100, "SOKOBAN",     "Push the boxes");
    drawButton(20, 540, 440, 100, "READER",      "Read ebooks");

    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // Instructions
    // M5.Display.setFont(&fonts::FreeSans9pt7b);
    // M5.Display.drawString("Hold BtnA = Menu",    20, 705);
    // M5.Display.drawString("Hold BtnB = Restart", 20, 725);
    // M5.Display.drawString("Hold BtnA + BtnB = Sleep/Wake", 20, 745);

    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString("MONO Launcher v1.0", 20, 760);

    endDisplay();
}

// ============================================================
// Instruction modal — drawn on top of the current screen
// ============================================================
// Caller passes a list of lines. The modal is dismissed by
// tapping anywhere on the screen.
void showInfoModal(const char* title, const char* const* lines, int lineCount) {
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    beginDisplay();
    clearScreen();

    M5.Display.fillRect(0, 0, 480, 800, TFT_WHITE);

    // Border
    M5.Display.drawRect(10, 10, 460, 780, TFT_BLACK);
    M5.Display.drawRect(11, 11, 458, 778, TFT_BLACK);

    // Title
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.drawString(title, 40, 40);

    // Horizontal rule
    M5.Display.drawLine(40, 90, 440, 90, TFT_BLACK);

    // Body lines
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    int y = 120;
    for (int i = 0; i < lineCount; i++) {
        M5.Display.drawString(lines[i], 40, y);
        y += 36;
    }

    // Footer hint
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.drawString("Tap anywhere to close", 40, 760);

    // M5.Display.display();   // force immediate flush
    endDisplay();
}

void showInfoModalAndWait(const char* title, const char* const* lines, int lineCount) {
    showInfoModal(title, lines, lineCount);

    // Wait for the tapping finger to lift
    while (M5.Touch.getCount() > 0) {
        M5.update();
        delay(20);
    }
    delay(150);

    // Wait for a fresh tap
    while (true) {
        M5.update();
        if (M5.Touch.getCount() > 0) {
            delay(50);   // let the tap settle
            while (M5.Touch.getCount() > 0) {
                M5.update();
                delay(20);
            }
            break;
        }
        delay(50);
    }

    resetTouch();
    touchGlobalActivity();
}

// ============================================================
// Confirm dialog — modal Yes/No popup drawn over the current screen
// ============================================================
bool confirmDialog(const char* title,
                   const char* message,
                   const char* yesLabel,
                   const char* noLabel) {

    // Modal geometry — centered, comfortably sized for 480x800 portrait.
    const int W = 360;
    const int H = 260;
    const int X = (480 - W) / 2;
    const int Y = (800 - H) / 2;

    // Button geometry inside the modal.
    const int BTN_H   = 60;
    const int BTN_W   = (W - 60) / 2;   // two buttons + margins
    const int BTN_Y   = Y + H - BTN_H - 24;
    const int YES_X   = X + 20;
    const int NO_X    = YES_X + BTN_W + 20;

    // ---- Draw the modal on top of whatever is currently on screen.
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    beginDisplay();

    // Card background + border
    M5.Display.fillRect(X, Y, W, H, TFT_WHITE);
    M5.Display.drawRect(X, Y, W, H, TFT_BLACK);
    M5.Display.drawRect(X + 1, Y + 1, W - 2, H - 2, TFT_BLACK);

    // Title
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    int tw = M5.Display.textWidth(title);
    M5.Display.drawString(title, X + (W - tw) / 2, Y + 24);

    // Horizontal rule under title
    M5.Display.drawLine(X + 20, Y + 78, X + W - 20, Y + 78, TFT_BLACK);

    // Message — up to two lines, centered
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    String msg = message ? message : "";

    // Split on '\n' if caller passed a two-line message.
    String line1 = msg, line2 = "";
    int nl = msg.indexOf('\n');
    if (nl >= 0) {
        line1 = msg.substring(0, nl);
        line2 = msg.substring(nl + 1);
    }

    int lh = M5.Display.fontHeight();
    int msgY = Y + 100;

    tw = M5.Display.textWidth(line1);
    M5.Display.drawString(line1, X + (W - tw) / 2, msgY);
    if (line2.length() > 0) {
        tw = M5.Display.textWidth(line2);
        M5.Display.drawString(line2, X + (W - tw) / 2, msgY + lh + 6);
    }

    // Buttons
    M5.Display.fillRoundRect(YES_X, BTN_Y, BTN_W, BTN_H, 8, TFT_BLACK);
    M5.Display.drawRoundRect(YES_X, BTN_Y, BTN_W, BTN_H, 8, TFT_BLACK);

    M5.Display.fillRoundRect(NO_X, BTN_Y, BTN_W, BTN_H, 8, TFT_WHITE);
    M5.Display.drawRoundRect(NO_X, BTN_Y, BTN_W, BTN_H, 8, TFT_BLACK);

    // Button labels
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);

    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    tw = M5.Display.textWidth(yesLabel);
    int th = M5.Display.fontHeight();
    M5.Display.drawString(yesLabel,
                          YES_X + (BTN_W - tw) / 2,
                          BTN_Y + (BTN_H - th) / 2);

    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    tw = M5.Display.textWidth(noLabel);
    M5.Display.drawString(noLabel,
                          NO_X + (BTN_W - tw) / 2,
                          BTN_Y + (BTN_H - th) / 2);

    endDisplay();

    // ---- Wait for the finger that triggered this dialog to lift.
    // (Same pattern as showInfoModalAndWait — critical because the
    // triggering event was a button hold, and some panels keep the
    // touch controller reporting a stale press.)
    M5.update();
    while (M5.Touch.getCount() > 0) {
        M5.update();
        delay(20);
    }
    delay(150);

    // ---- Wait for a fresh tap.
    int tapX = -1, tapY = -1;
    while (true) {
        M5.update();
        if (M5.Touch.getCount() > 0) {
            auto d = M5.Touch.getDetail(0);
            if (d.isPressed()) {
                mapTouchToDisplay(d.x, d.y, tapX, tapY);
            }
            while (M5.Touch.getCount() > 0) {
                M5.update();
                delay(20);
            }
            break;
        }
        delay(20);
    }

    resetTouch();
    touchGlobalActivity();

    // ---- Interpret the tap.
    if (tapX < 0 || tapY < 0) return false;

    // Tap on YES
    if (tapX >= YES_X && tapX <= YES_X + BTN_W &&
        tapY >= BTN_Y && tapY <= BTN_Y + BTN_H) {
        return true;
    }

    // Tap on NO (or anywhere else — including outside the card)
    return false;
}

// ============================================================
// Brightness
// ============================================================
void cycleBrightness() {
    static int brightnessIndex = 2;

    brightnessIndex = (brightnessIndex + 1) % BRIGHTNESS_COUNT;
    int newBrightness = BRIGHTNESS_LEVELS[brightnessIndex];
    M5.Display.setBrightness(newBrightness);

    Serial.printf("Brightness: %d (%d%%)\n",
                  newBrightness,
                  (newBrightness * 100) / 255);
}

// ============================================================
// Launcher loop
// ============================================================
void launcherLoop() {
    if (M5.BtnB.wasClicked()) {
        cycleBrightness();
        return;
    }

    int x, y;
    if (getTouch(x, y)) {
        // Info button → show modal
        if (hitInfoButton(x, y)) {
            static const char* const launcherHelp[] = {
                "Tap an app = Open it",
                "BtnB (short) = Brightness",
                "",
                "Hold BtnA + BtnB = Sleep / Wake",
            };
            showInfoModalAndWait("MONO Launcher v1.0", launcherHelp, 4);

            // Redraw the launcher
            drawLauncher();
            return;
        }

        // App buttons
        if (x >= 20 && x <= 440 && y >= 120 && y <= 220) { enterReversi();     return; }
        if (x >= 20 && x <= 440 && y >= 235 && y <= 335) { enterGame2048();    return; }
        if (x >= 20 && x <= 440 && y >= 350 && y <= 450) { enterMinesweeper(); return; }
        if (x >= 20 && x <= 440 && y >= 465 && y <= 565) { enterSokoban();     return; }
        if (x >= 20 && x <= 440 && y >= 580 && y <= 680) { enterReader();      return; }
        resetTouch();
    }
}
