#ifndef READER_H
#define READER_H

// #include "appmode.h"
// #include "appstate.h"
// #include "display.h"
// #include "storage.h"
// #include "touch.h"
#include <FS.h>
#include <LittleFS.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <SD.h>
#include <WString.h>
#include <vector>

// ---- Resume cache ---------------------------------------------------------
// One record per book. Old single-record files are still readable and get
// rewritten in the multi-record format on the next save.
struct ResumeInfo {
    bool valid = false;
    String book;
    uint32_t size = 0;
    uint32_t offset = 0;
    uint32_t permille = 0;
    std::vector<uint32_t> recent;
};

ResumeInfo loadResumeInfo(const String& bookPath = "");
void saveResumePosition(bool force = false);
void forgetResumeFor(const String& bookPath);

// Reader globals
extern std::vector<String> books;
extern int readerSelection;
extern int readerScroll;
extern String readerView;
extern String currentBookPath;
extern String currentBookName;
extern int currentPageNum;
extern int totalPages;
extern int readerFontSize;
extern int readerLineHeight;
extern int readerGlyphW;        // measured CJK glyph width (fallback)
extern int readerAsciiW;        // measured ASCII glyph width (fallback)
extern int readerFontLevel;     // 0=small, 1=medium, 2=large

void applyReaderFont();
void changeReaderFontSize(int dir);
void loadReaderPrefs();
void saveReaderPrefs();

void scanBooks();
void scanDirectory(File& dir, String path);
void drawBrowser();
void openBook(int idx);
void drawPage();
void readerLoop();

// External
// extern void enterLauncher();
extern void showInfoModalAndWait(const char* title, const char* const* lines, int lineCount);

#endif
