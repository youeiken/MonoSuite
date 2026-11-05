#ifndef READER_FONT_H
#define READER_FONT_H

#include <M5Unified.h>
#include <OpenFontRender.h>

bool readerFontInit(const char* path);
void readerFontUnload();

// 0 = small, 1 = medium, 2 = large
void readerFontSetLevel(int level);
int  readerFontGetLevel();
unsigned int readerFontGetSize();

// Draw / measure helpers
void     readerFontDraw(const char* utf8, int x, int y);
uint32_t readerFontMeasure(const char* utf8);

// Current line height for the active level (in pixels)
int readerFontLineHeight();

// Base line height of the current font size, before spacing multiplier.
int readerFontBaseLineHeight();

#endif
