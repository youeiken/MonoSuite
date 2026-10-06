#include "reader_font.h"
#include "ofr_sd_preset.h"

#include <OpenFontRender.h>
#include <SD.h>

static OpenFontRender s_render;
static bool s_fontLoaded = false;

static uint8_t* s_fontBuffer = nullptr;
static size_t   s_fontBufferSize = 0;

// Three base sizes. Line height is computed from the size, so the reader
// can apply a spacing multiplier on top.
static const unsigned int kSizes[3] = { 22, 30, 38 };

static int s_level = 1;

bool readerFontInit(const char* path) {
    if (s_fontLoaded) return true;

    File fontFile = SD.open(path, FILE_READ);
    if (!fontFile) {
        Serial.printf("[readerFont] Cannot open %s\n", path);
        return false;
    }

    size_t fontSize = fontFile.size();
    Serial.printf("[readerFont] TTF size = %u bytes\n", (unsigned)fontSize);

    s_fontBuffer = (uint8_t*)heap_caps_malloc(fontSize, MALLOC_CAP_SPIRAM);
    if (!s_fontBuffer) {
        Serial.println("[readerFont] PSRAM alloc failed");
        fontFile.close();
        return false;
    }

    size_t got = fontFile.read(s_fontBuffer, fontSize);
    fontFile.close();

    if (got != fontSize) {
        Serial.printf("[readerFont] Short read: %u / %u\n",
                      (unsigned)got, (unsigned)fontSize);
        heap_caps_free(s_fontBuffer);
        s_fontBuffer = nullptr;
        return false;
    }

    s_fontBufferSize = fontSize;

    s_render.setSerial(Serial);
    FT_Error err = s_render.loadFont(s_fontBuffer, s_fontBufferSize);
    Serial.printf("[readerFont] loadFont -> FT_Error = %d\n", (int)err);

    if (err != 0) {
        heap_caps_free(s_fontBuffer);
        s_fontBuffer = nullptr;
        s_fontBufferSize = 0;
        s_fontLoaded = false;
        return false;
    }

    s_render.setDrawer(M5.Display);
    s_render.setFontColor(TFT_BLACK, TFT_WHITE);
    s_render.setFontSize(kSizes[s_level]);

    s_fontLoaded = true;
    Serial.printf("[readerFont] loaded, size = %u\n", kSizes[s_level]);
    return true;
}

void readerFontUnload() {
    if (!s_fontLoaded) return;
    s_render.unloadFont();
    s_fontLoaded = false;

    if (s_fontBuffer) {
        heap_caps_free(s_fontBuffer);
        s_fontBuffer = nullptr;
        s_fontBufferSize = 0;
    }

    Serial.println("[readerFont] unloaded");
}

void readerFontSetLevel(int level) {
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    s_level = level;
    if (s_fontLoaded) {
        s_render.setFontSize(kSizes[s_level]);
        Serial.printf("[readerFont] level = %d, size = %u\n",
                      s_level, kSizes[s_level]);
    }
}

int readerFontGetLevel() { return s_level; }
unsigned int readerFontGetSize() { return kSizes[s_level]; }

void readerFontDraw(const char* utf8, int x, int y) {
    if (!s_fontLoaded) return;
    s_render.drawString(utf8, x, y);
}

uint32_t readerFontMeasure(const char* utf8) {
    if (!s_fontLoaded || utf8 == nullptr) return 0;

    // OpenFontRender's getTextWidth uses printf-style formatting. A raw
    // '%' in the input would be misinterpreted as a format specifier, so
    // escape any '%' before handing the string over.
    String safe(utf8);
    if (safe.indexOf('%') >= 0) {
        safe.replace("%", "%%");
    }
    return s_render.getTextWidth("%s", safe.c_str());
}

int readerFontLineHeight() {
    // Base line height = font size * 1.25. The reader multiplies this by
    // the user's line-spacing preference on top.
    return (int)((float)kSizes[s_level] * 1.25f);
}

int readerFontBaseLineHeight() {
    return readerFontLineHeight();
}
