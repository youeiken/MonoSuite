// ============================================================
// M5Stack Paper Mono Lite - Hardware Reference
// ============================================================
// Display:    480x800 e-paper (EPD)
// Touch:      GT911 (I2C)
//
// Buttons:
//   BtnA (USER_KEY1) -> GPIO 2
//   BtnB (USER_KEY2) -> GPIO 3
//
// microSD (SDMMC 4-bit mode):
//   CMD  -> GPIO 12
//   CLK  -> GPIO 13
//   DAT0 -> GPIO 11
//   DAT1 -> GPIO 10
//   DAT2 -> GPIO 9
//   DAT3 -> GPIO 8
//   TF_EN  -> M5IOE1 PYG14
//   TF_DET -> M5IOE1 PYG1
// ============================================================

// Include all module headers
#include "appmode.h"
#include "appstate.h"
#include "display.h"
#include "fontconfig.h"
#include "game2048.h"
#include "launcher.h"
#include "minesweeper.h"
#include "reader.h"
#include "reversi.h"
#include "sokoban.h"
#include "storage.h"
#include "time_sync.h"
#include "touch.h"
#include "wifi_setup.h"

// #pragma GCC diagnostic ignored "-fpermissive"
#include <FS.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <SD.h>
#include <SD_MMC.h>

// Slim down the built-in CJK fonts.
// We only need Kanji/Kana for the book-list browser and short labels.
// The actual reading uses a TTF loaded from the SD card.
// #define EFONT_USE_JA_MINI   1
// #define EFONT_USE_TW_MINI   0
// #define EFONT_USE_KR_MINI   0
// #define EFONT_USE_SC_MINI   0

// ---- shared mode state ----
#define SLEEP_MS         180000
#define MODE_LAUNCHER    0
#define MODE_REVERSI     1
#define MODE_GAME2048    2
#define MODE_MINESWEEPER 3
#define MODE_SOKOBAN     4
#define MODE_READER      5

int currentMode = MODE_LAUNCHER;
int sleepLastMinute = -1;

// ---- shared display / light ----
#define FRONT_LIGHT_DEFAULT 96
// int frontlight = FRONT_LIGHT_DEFAULT;
// const int FRONTLIGHT_LEVELS[] = {0, 32, 64, 96, 128, 192, 255};

// ============================================================
// Game-activity refresh — knows which mode is active, so it
// belongs here rather than in appstate.
// ============================================================
void touchGameActivity() {
    switch (currentMode) {
        case MODE_REVERSI:     rs_lastActivity = millis(); break;
        case MODE_GAME2048:    g2048_lastActivity = millis(); break;
        case MODE_MINESWEEPER: ms_lastActivity = millis(); break;
        case MODE_SOKOBAN:     sk_lastActivity = millis(); break;
    }
}

// ============================================================
// Two-button chord helper
// ============================================================
// Returns true once when A and B have been held together
// continuously for at least `holdMs`. Must be called every loop.
static bool chordTriggered(unsigned long holdMs = 1000) {
    static unsigned long chordStart = 0;
    bool both = M5.BtnA.isPressed() && M5.BtnB.isPressed();

    if (!both) {
        chordStart = 0;
        return false;
    }
    if (chordStart == 0) {
        chordStart = millis();
        return false;
    }
    if (millis() - chordStart >= holdMs) {
        chordStart = 0;      // consume the trigger
        return true;
    }
    return false;
}

// ============================================================
// Application entry functions
// ============================================================

void enterLauncher() {
    currentMode = MODE_LAUNCHER;
    setSleeping(false);
    M5.Display.setBrightness(FRONT_LIGHT_DEFAULT);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);

    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();

    drawLauncher();
}

void enterReversi() {
    currentMode = MODE_REVERSI;
    setSleeping(false);
    M5.Display.setBrightness(FRONT_LIGHT_DEFAULT);

    if (rs_initialized) {
        rs_lastActivity = millis();
        redrawReversiQuality();       // was redrawGameQuality()
    } else {
        newReversiGame();             // was newGame()
    }
}

void enterGame2048() {
    currentMode = MODE_GAME2048;
    setSleeping(false);
    M5.Display.setBrightness(FRONT_LIGHT_DEFAULT);

    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();

    if (!g2048_initialized) {
        initGame2048();
    }
    g2048_lastActivity = millis();
    redrawGame2048Quality();
}

void enterMinesweeper() {
    currentMode = MODE_MINESWEEPER;
    setSleeping(false);
    M5.Display.setBrightness(FRONT_LIGHT_DEFAULT);

    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();

    if (!ms_initialized) {
        initMinesweeper();
    }
    ms_lastActivity = millis();
    redrawMinesweeperQuality();
}

void enterSokoban() {
    currentMode = MODE_SOKOBAN;
    setSleeping(false);
    M5.Display.setBrightness(FRONT_LIGHT_DEFAULT);

    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();

    if (!sk_initialized) {
        initSokoban();
    }
    sk_lastActivity = millis();
    redrawSokobanQuality();
}

static bool booksScanned = false;

void enterReader() {    
    currentMode = MODE_READER;
    setSleeping(false);
    M5.Display.setBrightness(FRONT_LIGHT_DEFAULT);
    // M5.Display.setEpdMode(epd_mode_t::epd_quality);

    // Turn off LED when not in game
    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();

    // initReaderFont(); // uncomment it to load /fonts/font24.bin
    if (!booksScanned) {
        scanBooks();
        booksScanned = true;
    }
    readerSelection = 0;
    readerScroll = 0;
    readerView = "list";
    drawBrowser(); 
}

// ============================================================
// SD Card Initialization - Targeted for M5Stack PaperMono Lite
// ============================================================

SPIClass sdSPI(FSPI);

bool initSDCard() {
    Serial.println("Preparing PaperMono-Lite specialized filesystem interface...");
    delay(250);

    auto mosi = M5.getPin(m5::pin_name_t::sd_spi_mosi);
    auto miso = M5.getPin(m5::pin_name_t::sd_spi_miso);
    auto sclk = M5.getPin(m5::pin_name_t::sd_spi_sclk);
    auto cs   = M5.getPin(m5::pin_name_t::sd_spi_cs);

    Serial.printf("[SD Pins Debug] -> MOSI:%d | MISO:%d | SCLK:%d | CS:%d\n",
                  mosi, miso, sclk, cs);

    if (cs < 0 || mosi < 0 || miso < 0 || sclk < 0) {
        Serial.println("❌ Critical Error: M5Unified could not load PaperMono Pin Maps.");
        return false;
    }

    SD.end();
    sdSPI.begin(sclk, miso, mosi, cs);

    Serial.println("Mounting via dedicated SPI instance at 15MHz...");
    if (SD.begin(cs, sdSPI, 15000000)) {
        Serial.println("✅ SD Card initialized successfully via target FSPI!");
        return true;
    }

    Serial.println("⚠️ 15MHz failed. Attempting 4MHz safe initialization frequency...");
    if (SD.begin(cs, sdSPI, 4000000)) {
        Serial.println("✅ SD Card initialized successfully in safe-mode (4MHz)!");
        return true;
    }

    Serial.println("❌ Hard failure: SD card rejected communications over target FSPI lines.");
    return false;
}

// ============================================================
// Global Sleep / Wake Functions
// ============================================================

int modeBeforeSleep = MODE_LAUNCHER;

void drawSleepClock() {
    String timeStr = displayTimeFromRTC();

    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    const int pad = 18;          // <-- increase this for more padding
    const int border = 2;        // thickness of the white outline

    int tw = M5.Display.textWidth(timeStr);
    int th = M5.Display.fontHeight();

    // Compute the text's top-left so the panel is centred around it
    int x = 440 - tw;
    int y = 48;

    // Panel dimensions
    int panelW = tw  + 2 * pad;
    int panelH = th  + 2 * pad;
    int panelX = x - pad;
    int panelY = y - pad - 2;

    // Inverted: black panel, white text, white outline
    M5.Display.fillRect(panelX, panelY, panelW, panelH, TFT_BLACK);

    // Optional thin white outline so the panel stands out
    for (int i = 0; i < border; i++) {
        M5.Display.drawRect(panelX - i, panelY - i,
                            panelW + 2 * i, panelH + 2 * i,
                            TFT_WHITE);
    }

    M5.Display.drawString(timeStr, x, y);
}

void enterSleep() {
    setSleeping(true);
    modeBeforeSleep = currentMode;
    sleepLastMinute = -1;

    M5.Display.setBrightness(0);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);

    uint8_t* bmpBuffer = nullptr;
    size_t fileSize = 0;
    const char* bmpPath = "/sleep.bmp";

    File targetFile;
    storageOpenRead(bmpPath, targetFile);

    if (targetFile) {
        fileSize = targetFile.size();
        if (fileSize > 0) {
            bmpBuffer = (uint8_t*)heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM);
            if (bmpBuffer != nullptr) {
                targetFile.read(bmpBuffer, fileSize);
                Serial.printf("[Sleep] Loaded %d bytes into PSRAM\n", fileSize);
            }
        }
        targetFile.close();
    }

    beginDisplay();
    clearScreen();

    bool bmpRendered = false;
    if (bmpBuffer != nullptr && fileSize > 0) {
        bmpRendered = M5.Display.drawBmp(bmpBuffer, fileSize, 0, 0);
        heap_caps_free(bmpBuffer);
    }

    if (!bmpRendered) {
        M5.Display.setFont(&fonts::FreeSans24pt7b);
        M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
        M5.Display.drawString("Sleeping...", 120, 300);
        M5.Display.setFont(&fonts::FreeSans12pt7b);
        M5.Display.drawString("Tap or press to wake", 140, 400);
    }

    drawSleepClock();

    M5.Display.display();
    endDisplay();

    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();

    Serial.println("💤 Entering sleep mode");
}

void wakeUp() {
    if (!isSleeping()) return;
    setSleeping(false);

    M5.Display.setBrightness(FRONT_LIGHT_DEFAULT);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    touchGlobalActivity();

    Serial.printf("☀️ Waking up to mode %d\n", modeBeforeSleep);

    switch (modeBeforeSleep) {
        case MODE_REVERSI:
            enterReversi();
            break;
        case MODE_GAME2048:
            enterGame2048();
            break;
        case MODE_MINESWEEPER:
            enterMinesweeper();
            break;
        case MODE_SOKOBAN:
            enterSokoban();
            break;
        case MODE_READER:
            enterReader();
            break;
        case MODE_LAUNCHER:
        default:
            enterLauncher();
            break;
    }
}

// ============================================================
// Setup
// ============================================================

void setup() {
    auto cfg = M5.config();
    cfg.fallback_board = m5::board_t::board_M5PaperS3;
    M5.begin(cfg);

    M5.Display.setRotation(0);
    M5.Display.setBrightness(FRONT_LIGHT_DEFAULT);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);

    M5.Led.setAutoDisplay(false);
    M5.Led.setBrightness(80);
    M5.Led.setAllColor(0, 0, 0);
    M5.Led.display();

    Serial.begin(115200);
    delay(1000);
    Serial.println("=== M5Stack Paper Mono Lite ===");
    Serial.printf("Display Size: %dx%d\n", M5.Display.width(), M5.Display.height());

    initTouchCalibration();

    setSdMounted(initSDCard());
    Serial.printf("[Boot] SD card: %s\n", isSdMounted() ? "✅ mounted" : "❌ NOT mounted");
    
    bool timeOk = ensureRtcTime(/*allowWifiFallback=*/true);
    Serial.printf("[Boot] RTC time: %s\n", timeOk ? "set" : "unset");

    // Sync NTP only if the RTC looks unset
    bool needSync = true;
    if (M5.Rtc.isEnabled()) {
        auto dt = M5.Rtc.getDateTime();
        if (dt.date.year >= 2024) {
            needSync = false;
        }
    }

    // Try saved WiFi credentials
    /* bool wifiUp = tryConnectSavedWifi();
    Serial.printf("[Boot] WiFi: %s\n", wifiUp ? "connected" : "offline");

    if (wifiUp && needSync) {
        syncTimeFromNTP();
    } */

    enterLauncher();

    Serial.printf("[RTC] Enabled: %d\n", M5.Rtc.isEnabled());
}

// ============================================================
// Loop
// ============================================================

void loop() {
    M5.update();

    // ============================================================
    // SLEEP HANDLING
    // ============================================================
    if (isSleeping()) {
        // Wake only on A+B chord
        if (chordTriggered(300)) {     // shorter hold for wake = snappier
            wakeUp();
            return;
        }

        // Swallow any stray touches so they don't queue up
        int x, y;
        if (getTouch(x, y)) {
            resetTouch();
        }

        // Redraw the clock when the minute changes
        static unsigned long lastClockCheck = 0;

        if (millis() - lastClockCheck >= 1000) {
            lastClockCheck = millis();

            String timeStr = displayTimeFromRTC();

            int colonPos = timeStr.indexOf(':');
            if (colonPos > 0) {
                int minute = timeStr.substring(colonPos + 1).toInt();

                if (minute != sleepLastMinute) {
                    sleepLastMinute = minute;

                    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
                    beginDisplay();
                    drawSleepClock();
                    endDisplay();

                    Serial.printf("[Sleep] Clock updated: %s\n", timeStr.c_str());
                }
            }
        }

        delay(100);
        return;
    }

    // Two-button chord -> manual sleep (in any mode, including launcher)
    if (chordTriggered(1000)) {
        enterSleep();
        return;
    }

    bool bothHeld = M5.BtnA.isPressed() && M5.BtnB.isPressed();

    if (bothHeld) {
        // While both are held, skip the mode dispatch entirely so
        // game loops don't see wasHold/wasClicked on either button.
        if (chordTriggered(1000)) {
            enterSleep();
            return;
        }
        delay(30);
        return;
    }

    // Check idle timeout (applies to all modes)
    if (millis() - lastActivityMs() > SLEEP_MS) {
        enterSleep();
        return;
    }

    // ============================================================
    // ACTIVITY TRACKING
    // ============================================================
    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.Touch.getCount() > 0) {
        touchGlobalActivity();
        touchGameActivity();
    }

    // ============================================================
    // MODE DISPATCH
    // ============================================================
    switch(currentMode) {
        case MODE_LAUNCHER:
            launcherLoop();
            break;
        case MODE_REVERSI:
            reversiLoop();
            break;
        case MODE_GAME2048:
            game2048Loop();
            break;
        case MODE_MINESWEEPER:
            minesweeperLoop();
            break;
        case MODE_SOKOBAN:
            sokobanLoop();
            break;
        case MODE_READER:
            readerLoop();
            break;
    }
    delay(30);
}
