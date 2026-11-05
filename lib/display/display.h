#ifndef DISPLAY_H
#define DISPLAY_H

#include <FS.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <SD_MMC.h>

#define EPD_QUALITY 0
#define EPD_TEXT    1
#define EPD_FAST    2
#define EPD_FASTEST 3

inline void setBrightness(int level) {
    M5.Display.setBrightness(level);
}

inline void clearScreen(uint16_t color = TFT_WHITE) {
    M5.Display.fillScreen(color);
}

inline void beginDisplay() {
    M5.Display.startWrite();
}

inline void endDisplay() {
    M5.Display.endWrite();
}

inline void refreshDisplay() {
    // Modern display pipeline manages flushing automatically
}

inline bool loadBMP(const char* path, int x, int y, int w = -1, int h = -1) {
    if (!LittleFS.exists(path) && !SD_MMC.exists(path)) {
        return false;
    }
    return M5.Display.drawBmpFile(path, x, y, w, h);
}

#endif
