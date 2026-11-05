#include "reader.h"
#include "launcher.h"

#include "appmode.h"
#include "appstate.h"
#include "display.h"
#include "storage.h"
#include "touch.h"
#include "ebook_decode.h"
#include "fontconfig.h"
#include "reader_font.h"
#include <algorithm>
#include <LittleFS.h>
#include <SD_MMC.h>

// ============================================================
// Constants
// ============================================================
static const int MARGIN_LEFT         = 20;
static const int MARGIN_TOP          = 70;
static const int MAX_WIDTH           = 450;
static const int FOOTER_Y            = 750;
static const int MAX_Y               = FOOTER_Y - 10;
static const int kReaderFontCount    = 3;
static const int RESUME_MAX_BOOKS    = 16;
static const int RESUME_RING_MAX     = 20;   // how many recent pages we remember
static const char* FONT_PATH         = "/fonts/NotoSansTC-Subset.ttf";
static const char* RESUME_PATH       = "/reader/reader_resume.txt";
static const char* READER_PREFS_PATH = "/reader/reader_prefs.txt";
static bool forceQualityNextPage     = false;

// Resume save throttle
static unsigned long lastResumeSaveMs = 0;
static int           lastSavedPage    = -1;
static const unsigned long RESUME_SAVE_MIN_INTERVAL_MS = 2000;

// ============================================================
// Global state
// ============================================================
std::vector<String> books;
int readerSelection    = 0;
int readerScroll       = 0;
String readerView      = "list";
String currentBookPath = "";
String currentBookName = "";

int currentPageNum = 0;
std::vector<uint32_t> pageOffsets;
int totalPages = 0;

// ============================================================
// Book buffer — owned by reader.cpp, written by the async worker.
//
// INVARIANT: bookData / bookDataSize may only be read while
// !bookLoadInProgress. bookLoadTask() runs on another core and
// clears then rewrites these; the main reader loop must check
// bookLoadInProgress before any read. Violating this is a
// use-after-free waiting to happen.
//
// The reader's own functions that touch bookData
// (drawPage, saveResumePosition, pageNext, pagePrev) are only
// called from readerLoop after the guard, so they are safe
// by construction. Any new code path that reads these must also
// go through the guard.
// ============================================================
static uint8_t* bookData     = nullptr;
static size_t   bookDataSize = 0;

// Font level defaults (overridden by loadReaderPrefs)
int readerFontLevel  = 1;     // medium
int readerFontSize   = 28;
int readerLineHeight = 40;
int readerGlyphW     = 28;
int readerAsciiW     = 14;

// Input debounce
static unsigned long lastScrollTime = 0;
static const unsigned long SCROLL_DEBOUNCE_MS = 200;

// Touch tracking
static bool          rdTouchActive     = false;
static int           rdTouchStartX     = 0;
static int           rdTouchStartY     = 0;
static unsigned long rdTouchStartTime  = 0;
static bool          rdSwipeHandled    = false;

static const int           RD_SWIPE_MIN_DIST  = 40;
static const unsigned long RD_SWIPE_MAX_MS    = 900;
static const int           RD_SWIPE_DOMINANCE = 2;

enum RdGesture {
    RD_GESTURE_NONE = 0,
    RD_GESTURE_TAP,
    RD_GESTURE_SWIPE_UP,
    RD_GESTURE_SWIPE_DOWN,
    RD_GESTURE_SWIPE_LEFT,
    RD_GESTURE_SWIPE_RIGHT,
    RD_GESTURE_BACK,
    RD_GESTURE_INFO,
};

// ============================================================
// Font management
// ============================================================

void applyReaderFont() {
    if (readerFontLevel < 0) readerFontLevel = 0;
    if (readerFontLevel >= kReaderFontCount) readerFontLevel = kReaderFontCount - 1;

    readerFontSetLevel(readerFontLevel);
    readerFontSize   = readerFontGetSize();
    readerLineHeight = readerFontLineHeight();

    Serial.printf("[Reader] font level=%d size=%d lh=%d\n",
                  readerFontLevel, readerFontSize, readerLineHeight);
}

// dir: -1 = smaller, +1 = larger
void changeReaderFontSize(int dir) {
    forceQualityNextPage = true;

    int newLevel = readerFontLevel + dir;
    if (newLevel < 0) newLevel = 0;
    if (newLevel >= kReaderFontCount) newLevel = kReaderFontCount - 1;

    bool changed = (newLevel != readerFontLevel);
    readerFontLevel = newLevel;

    if (changed) {
        applyReaderFont();
        saveReaderPrefs();

        // Force repagination from the current page start.
        // Keep the ring entries up to and including the current page —
        // those byte offsets are still valid page starts, they'll just
        // render at the new font size. Discard entries after it, since
        // those were paginated for the old font size.
        if (!pageOffsets.empty() && currentPageNum >= 0) {
            int keep = currentPageNum + 1;
            if (keep > (int)pageOffsets.size()) keep = pageOffsets.size();
            pageOffsets.resize(keep);
            currentPageNum = pageOffsets.size() - 1;
            totalPages = pageOffsets.size();
        }
    }

    // Overlay (always shows, even at limit)
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    beginDisplay();
    M5.Display.fillRect(180, 420, 120, 60, TFT_WHITE);
    M5.Display.drawRect(180, 420, 120, 60, TFT_BLACK);
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    const char* label = (readerFontLevel == 0) ? "A-" :
                        (readerFontLevel == 1) ? "A"  : "A+";
    int tw = M5.Display.textWidth(label);
    M5.Display.drawString(label, 180 + (120 - tw) / 2, 432);
    endDisplay();

    if (changed) {
        drawPage();
        saveResumePosition(true);
    }
}

// ============================================================
// Forward declarations
// ============================================================
static void bookLoadTask(void* param);
static void checkBookLoadComplete();

// ============================================================
// Async book-load state
// ============================================================
static volatile bool  bookLoadInProgress = false;
static volatile bool  bookLoadComplete   = false;
static volatile bool  bookLoadSuccess    = false;

// ============================================================
// Book directory scanning
// ============================================================
void scanDirectory(File& dir, String path) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;

        String name = entry.name();
        if (name.startsWith(".")) { entry.close(); continue; }

        String fullPath = (path.length() > 0) ? (path + "/" + name) : ("/" + name);

        if (entry.isDirectory()) {
            File subDir = entry;
            scanDirectory(subDir, fullPath);
        } else {
            String low = name;
            low.toLowerCase();
            if (ebookIsSupported(low)) {
                books.push_back(fullPath);
            }
        }
        entry.close();
    }
}

void scanBooks() {
    books.clear();

    if (isSdMounted() && SD_MMC.exists("/books")) {
        File booksDir = SD_MMC.open("/books");
        if (booksDir && booksDir.isDirectory()) {
            scanDirectory(booksDir, "/books");
            booksDir.close();
        }
    }

    if (LittleFS.begin(true)) {
        File root = LittleFS.open("/");
        if (root) {
            scanDirectory(root, "");
            root.close();
        }
    }

    std::sort(books.begin(), books.end());
    books.erase(std::unique(books.begin(), books.end()), books.end());

    Serial.printf("[Reader] Found %d books available across filesystems.\n", (int)books.size());
    for (size_t i = 0; i < books.size(); i++) {
        Serial.printf("[Reader]   book[%d] = %s\n", (int)i, books[i].c_str());
    }
    Serial.flush();
}

// ============================================================
// Glyph rendering (now via OpenFontRender)
// ============================================================
static int encodeUtf8(uint32_t code, char* buf) {
    int len = 0;
    if (code < 0x80) {
        buf[len++] = (char)code;
    } else if (code < 0x800) {
        buf[len++] = (char)(0xC0 | (code >> 6));
        buf[len++] = (char)(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        buf[len++] = (char)(0xE0 | (code >> 12));
        buf[len++] = (char)(0x80 | ((code >> 6) & 0x3F));
        buf[len++] = (char)(0x80 | (code & 0x3F));
    } else {
        buf[len++] = (char)(0xF0 | (code >> 18));
        buf[len++] = (char)(0x80 | ((code >> 12) & 0x3F));
        buf[len++] = (char)(0x80 | ((code >> 6) & 0x3F));
        buf[len++] = (char)(0x80 | (code & 0x3F));
    }
    buf[len] = 0;
    return len;
}

void drawEfontGlyph(uint32_t code, int x, int y) {
    char buf[5];
    encodeUtf8(code, buf);
    readerFontDraw(buf, x, y);
}

// ============================================================
// UTF-8 decode
// ============================================================
static uint32_t decodeUtf8FromBuffer(const uint8_t* buf, size_t size, uint32_t& pos) {
    if (pos >= size) return 0;

    int c = buf[pos++];
    if (c < 0x80) return c;

    uint32_t code = 0;
    int extraBytes = 0;

    if ((c & 0xE0) == 0xC0) { code = c & 0x1F; extraBytes = 1; }
    else if ((c & 0xF0) == 0xE0) { code = c & 0x0F; extraBytes = 2; }
    else if ((c & 0xF8) == 0xF0) { code = c & 0x07; extraBytes = 3; }
    else return 0x20;

    for (int i = 0; i < extraBytes; i++) {
        if (pos >= size) return 0x20;
        int b = buf[pos++];
        if ((b & 0xC0) != 0x80) return 0x20;
        code = (code << 6) | (b & 0x3F);
    }
    return code;
}

// ============================================================
// Async worker
// ============================================================
static void bookLoadTask(void* param) {
    String path = *(String*)param;
    delete (String*)param;

    if (bookData) { heap_caps_free(bookData); bookData = nullptr; bookDataSize = 0; }

    bookLoadSuccess = ebookLoad(path, &bookData, &bookDataSize);
    bookLoadComplete = true;
    bookLoadInProgress = false;
    vTaskDelete(NULL);
}

// ============================================================
// openBook()
// ============================================================
void openBook(int idx) {
    if (idx < 0 || idx >= (int)books.size()) return;

    currentBookPath = books[idx];
    currentBookName = currentBookPath.substring(currentBookPath.lastIndexOf('/') + 1);

    // Show loading screen
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    beginDisplay();
    clearScreen();
    M5.Display.setFont(&fonts::FreeSans18pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.drawString("Loading...", 140, 300);
    endDisplay();

    // Load the TTF once, before the async task spawns.
    if (!readerFontInit(FONT_PATH)) {
        Serial.println("[Reader] TTF load failed — falling back to efont");
    }

    bookLoadInProgress = true;
    bookLoadComplete = false;
    bookLoadSuccess = false;

    String* pathCopy = new String(currentBookPath);
    xTaskCreatePinnedToCore(
        bookLoadTask, "bookLoad", 32768, pathCopy, 1, NULL, 0
    );
}

// ============================================================
// Resume cache — multi-book
// ============================================================
//
// File format (one record per book, blank line between records):
//
//   book=/books/foo.epub
//   size=12345
//   offset=6789
//   permille=550
//   recent=100,200,300
//
//   book=/books/bar.txt
//   size=9999
//   offset=1234
//   permille=123
//   recent=10,20
//
// Records are stored most-recently-used first. On save, the touched
// record moves to the top. When the cap is exceeded, the tail is
// dropped.

struct ResumeRecord {
    String   book;
    uint32_t size = 0;
    uint32_t offset = 0;
    uint32_t permille = 0;
    std::vector<uint32_t> recent;
};

static bool openResumeFileForRead(File& f) {
    if (isSdMounted() && SD_MMC.exists(RESUME_PATH)) {
        f = SD_MMC.open(RESUME_PATH, FILE_READ);
        return (bool)f;
    }
    if (LittleFS.exists(RESUME_PATH)) {
        f = LittleFS.open(RESUME_PATH, FILE_READ);
        return (bool)f;
    }
    return false;
}

static bool openResumeFileForWrite(File& f) {
    if (isSdMounted()) {
        f = SD_MMC.open(RESUME_PATH, FILE_WRITE);
    } else {
        f = LittleFS.open(RESUME_PATH, FILE_WRITE);
    }
    return (bool)f;
}

static std::vector<ResumeRecord> readResumeRecords() {
    std::vector<ResumeRecord> out;

    File f;
    if (!openResumeFileForRead(f)) return out;

    String s = f.readString();
    f.close();

    ResumeRecord cur;
    bool haveCur = false;

    int lineStart = 0;
    while (lineStart < s.length()) {
        int lineEnd = s.indexOf('\n', lineStart);
        if (lineEnd < 0) lineEnd = s.length();
        String line = s.substring(lineStart, lineEnd);
        line.trim();

        if (line.startsWith("book=")) {
            if (haveCur && cur.book.length()) out.push_back(cur);
            cur = ResumeRecord();
            cur.book = line.substring(5);
            haveCur = true;
        } else if (line.startsWith("size=") && haveCur) {
            cur.size = (uint32_t)line.substring(5).toInt();
        } else if (line.startsWith("offset=") && haveCur) {
            cur.offset = (uint32_t)line.substring(7).toInt();
        } else if (line.startsWith("permille=") && haveCur) {
            cur.permille = (uint32_t)line.substring(9).toInt();
        } else if (line.startsWith("recent=") && haveCur) {
            String csv = line.substring(7);
            int start = 0;
            while (start < (int)csv.length()) {
                int comma = csv.indexOf(',', start);
                if (comma < 0) comma = csv.length();
                String tok = csv.substring(start, comma);
                tok.trim();
                if (tok.length() > 0) {
                    cur.recent.push_back((uint32_t)tok.toInt());
                }
                start = comma + 1;
            }
        }

        lineStart = lineEnd + 1;
    }

    if (haveCur && cur.book.length()) out.push_back(cur);
    return out;
}

static void writeResumeRecords(const std::vector<ResumeRecord>& recs) {
    File f;
    if (!openResumeFileForWrite(f)) return;

    int n = (int)recs.size();
    if (n > RESUME_MAX_BOOKS) n = RESUME_MAX_BOOKS;

    for (int i = 0; i < n; i++) {
        const ResumeRecord& r = recs[i];
        if (r.book.length() == 0) continue;

        f.print("book=");       f.println(r.book);
        f.print("size=");       f.println((unsigned)r.size);
        f.print("offset=");     f.println((unsigned)r.offset);
        f.print("permille=");   f.println((unsigned)r.permille);
        f.print("recent=");
        for (size_t k = 0; k < r.recent.size(); k++) {
            f.print((unsigned)r.recent[k]);
            if (k + 1 < r.recent.size()) f.print(',');
        }
        f.println();
        f.println();
    }
    f.close();
}

ResumeInfo loadResumeInfo(const String& bookPath) {
    ResumeInfo r;
    r.valid = false;

    std::vector<ResumeRecord> recs = readResumeRecords();
    if (recs.empty()) return r;

    const ResumeRecord* hit = nullptr;
    if (bookPath.length() == 0) {
        hit = &recs.front();
    } else {
        for (const auto& rec : recs) {
            if (rec.book == bookPath) { hit = &rec; break; }
        }
    }
    if (!hit) return r;

    r.valid    = true;
    r.book     = hit->book;
    r.size     = hit->size;
    r.offset   = hit->offset;
    r.permille = hit->permille;
    r.recent   = hit->recent;
    return r;
}

// Save the current reading position for the currently-open book.
// Uses the OLD-reader globals (bookData / bookDataSize).
void saveResumePosition(bool force) {
    if (bookData == nullptr || bookDataSize == 0) return;
    if (pageOffsets.empty()) return;
    if (readerView != "read") return;
    if (currentBookPath.length() == 0) return;

    if (!force) {
        if (currentPageNum == lastSavedPage) return;
        if (lastResumeSaveMs != 0 &&
            (millis() - lastResumeSaveMs) < RESUME_SAVE_MIN_INTERVAL_MS) return;
    }

    uint32_t offset = pageOffsets[currentPageNum];
    uint32_t total  = (uint32_t)bookDataSize;
    uint32_t permille = (total > 0)
        ? (uint32_t)((uint64_t)offset * 1000 / total) : 0;

    ResumeRecord mine;
    mine.book     = currentBookPath;
    mine.size     = total;
    mine.offset   = offset;
    mine.permille = permille;

    int firstIdx = (currentPageNum >= RESUME_RING_MAX)
                       ? (currentPageNum - (RESUME_RING_MAX - 1))
                       : 0;
    for (int i = firstIdx; i <= currentPageNum; i++) {
        mine.recent.push_back(pageOffsets[i]);
    }

    std::vector<ResumeRecord> recs = readResumeRecords();

    std::vector<ResumeRecord> next;
    next.reserve(recs.size() + 1);
    next.push_back(mine);
    for (const auto& rec : recs) {
        if (rec.book == currentBookPath) continue;
        next.push_back(rec);
    }
    if ((int)next.size() > RESUME_MAX_BOOKS) {
        next.resize(RESUME_MAX_BOOKS);
    }

    writeResumeRecords(next);

    lastSavedPage    = currentPageNum;
    lastResumeSaveMs = millis();

    Serial.printf("[Resume] saved %s off=%u permille=%u (records=%d)\n",
                  currentBookPath.c_str(),
                  (unsigned)offset, (unsigned)permille,
                  (int)next.size());
}

void forgetResumeFor(const String& bookPath) {
    if (bookPath.length() == 0) return;

    std::vector<ResumeRecord> recs = readResumeRecords();
    std::vector<ResumeRecord> next;
    next.reserve(recs.size());
    for (const auto& rec : recs) {
        if (rec.book != bookPath) next.push_back(rec);
    }
    if (next.size() == recs.size()) return;
    writeResumeRecords(next);
}

// ============================================================
// Book load completion handler
// ============================================================
static void checkBookLoadComplete() {
    if (!bookLoadComplete) return;

    bookLoadComplete = false;

    if (bookLoadSuccess) {
        Serial.printf("[Reader] ==== BOOK LOAD SUMMARY ====\n");
        Serial.printf("[Reader] bookDataSize = %u bytes\n", (unsigned)bookDataSize);
        Serial.printf("[Reader] ========================\n");
        Serial.flush();

        // Handle UTF-8 BOM for plain text files
        uint32_t startByte = 0;
        String lower = currentBookPath;
        lower.toLowerCase();
        if (!lower.endsWith(".epub") && bookDataSize >= 3 &&
            bookData[0] == 0xEF && bookData[1] == 0xBB && bookData[2] == 0xBF) {
            startByte = 3;
        }

        ResumeInfo info = loadResumeInfo(currentBookPath);
        pageOffsets.clear();
        currentPageNum = 0;

        bool restored = false;

        if (info.valid && info.book == currentBookPath &&
            info.size == bookDataSize && !info.recent.empty()) {
            // Restore the recent-page ring
            for (uint32_t off : info.recent) {
                if (off < bookDataSize) {
                    pageOffsets.push_back(off);
                }
            }
            if (!pageOffsets.empty()) {
                currentPageNum = pageOffsets.size() - 1;
                restored = true;
                Serial.printf("[Resume] restored %d recent pages, current=%d\n",
                              (int)pageOffsets.size(), currentPageNum);
            }
        }

        if (!restored) {
            // Fallback: single resume offset
            uint32_t resumeOffset = startByte;
            if (info.valid && info.book == currentBookPath) {
                if (info.size == bookDataSize) {
                    resumeOffset = info.offset;
                    Serial.printf("[Resume] exact resume at offset %u\n",
                                  (unsigned)resumeOffset);
                } else if (info.permille > 0 && bookDataSize > 0) {
                    resumeOffset = (uint32_t)((uint64_t)bookDataSize * info.permille / 1000);
                    Serial.printf("[Resume] permille resume: permille=%u -> offset %u\n",
                                  (unsigned)info.permille, (unsigned)resumeOffset);
                }
            }
            if (resumeOffset >= bookDataSize) resumeOffset = startByte;
            pageOffsets.push_back(resumeOffset);
            currentPageNum = 0;
        }

        lastSavedPage = -1;
        lastResumeSaveMs = 0;

        totalPages = pageOffsets.size();
        readerView = "read";
        drawPage();
    } else {
        // Show error, then go back to the browser
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
        beginDisplay();
        clearScreen();
        M5.Display.setFont(&fonts::FreeSans12pt7b);
        M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
        M5.Display.drawString("Cannot open file:", 40, 340);
        M5.Display.setFont(&fonts::efontTW_24_b);
        M5.Display.drawString(currentBookName, 40, 380);
        endDisplay();
        delay(1500);
        drawBrowser();
    }

    rdTouchActive = false;
    rdSwipeHandled = true;
}

// ============================================================
// Page rendering
// ============================================================
static uint32_t mapToFullwidth(uint32_t cp) {
    switch (cp) {
        case ',':  return 0xFF0C;
        case '.':  return 0x3002;
        case '!':  return 0xFF01;
        case '?':  return 0xFF1F;
        case ':':  return 0xFF1A;
        case ';':  return 0xFF1B;
        case '(':  return 0xFF08;
        case ')':  return 0xFF09;
        case '<':  return 0x300A;
        case '>':  return 0x300B;
        case '[':  return 0x3010;
        case ']':  return 0x3011;
        case '{':  return 0x3008;
        case '}':  return 0x3009;
        default:   return cp;
    }
}

static void drawHeaderReader(const String& title,
                             const lgfx::IFont* font = &fonts::FreeSansBold18pt7b) {
    // CJK codepoints are 3 bytes each. Keep up to 36 bytes = 12 CJK chars.
    const int MAX_TITLE_BYTES = 32;

    String display = title;
    if (display.length() > MAX_TITLE_BYTES) {
        // Round down to the nearest 3-byte boundary to avoid mid-char cuts
        int cut = MAX_TITLE_BYTES;
        while (cut > 0 && ((uint8_t)display[cut] & 0xC0) == 0x80) {
            cut--;   // back up over a continuation byte
        }
        display = display.substring(0, cut) + "...";
    }
    drawHeader(display, font, true);
    // M5.Display.drawLine(0, 80, 480, 80, TFT_BLACK);
}

void drawPage() {
    if (bookData == nullptr || bookDataSize == 0) return;
    if (currentPageNum < 0 || currentPageNum >= (int)pageOffsets.size()) {
        Serial.printf("[Reader] Page offset %d out of range.\n", currentPageNum);
        return;
    }

    uint32_t currentFileIdx = pageOffsets[currentPageNum];

    static int pageFastCount = 0;
    if (forceQualityNextPage || pageFastCount >= 5) {
        M5.Display.setEpdMode(epd_mode_t::epd_quality);
        pageFastCount = 0;
        forceQualityNextPage = false;
    } else {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        pageFastCount++;
    }

    beginDisplay();
    clearScreen();

    // Header (book title)
    // String headerName = currentBookName;
    // headerName.replace(".txt", "");
    // headerName.replace(".epub", "");
    // drawHeaderReader(headerName, &fonts::efontTW_24_b);
    drawHeaderReader("", &fonts::efontTW_24_b);

    int cursorX = MARGIN_LEFT;
    int cursorY = MARGIN_TOP;

    while (currentFileIdx < bookDataSize && cursorY + readerLineHeight < MAX_Y) {
        uint32_t savedFileIdx = currentFileIdx;
        uint32_t characterCode = decodeUtf8FromBuffer(bookData, bookDataSize, currentFileIdx);
        if (characterCode == 0) break;

        if (characterCode == '\r' || characterCode == '\n') {
            int newlineCount = 0;

            if (characterCode == '\n') {
                newlineCount = 1;
            } else {
                newlineCount = 1;
                if (currentFileIdx < bookDataSize && bookData[currentFileIdx] == '\n') {
                    currentFileIdx++;
                }
            }

            while (currentFileIdx < bookDataSize) {
                uint8_t b = bookData[currentFileIdx];
                if (b == '\r' || b == '\n') {
                    newlineCount++;
                    currentFileIdx++;
                } else if (b == ' ' || b == '\t') {
                    uint32_t look = currentFileIdx;
                    while (look < bookDataSize &&
                        (bookData[look] == ' ' || bookData[look] == '\t')) {
                        look++;
                    }
                    if (look < bookDataSize &&
                        (bookData[look] == '\r' || bookData[look] == '\n')) {
                        currentFileIdx = look;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }

            int breaks = (newlineCount >= 2) ? 2 : 1;
            cursorX = MARGIN_LEFT;
            cursorY += breaks * readerLineHeight;
            continue;
        }

        uint32_t nextCp = 0;
        if (currentFileIdx < bookDataSize) {
            uint32_t probe = currentFileIdx;
            nextCp = decodeUtf8FromBuffer(bookData, bookDataSize, probe);
        }
        bool nextIsCJK = (nextCp >= 0x2E80 && nextCp <= 0x9FFF);

        if (characterCode < 0x80 && nextIsCJK) {
            characterCode = mapToFullwidth(characterCode);
        }

        char utf8Buf[5];
        encodeUtf8(characterCode, utf8Buf);

        int charW;
        if (characterCode >= 0x2E80) {
            charW = readerFontSize;
        } else if (characterCode == ' ') {
            charW = readerFontSize / 4;
        } else {
            charW = (int)readerFontMeasure(utf8Buf);
            if (charW <= 0) charW = readerFontSize / 2;
        }

        if (cursorX + charW > MARGIN_LEFT + MAX_WIDTH) {
            cursorX = MARGIN_LEFT;
            cursorY += readerLineHeight;
        }
        if (cursorY + readerLineHeight >= MAX_Y) {
            currentFileIdx = savedFileIdx;
            break;
        }

        int drawX = cursorX;
        int drawY = cursorY;

        bool isCJKPunct =
            (characterCode >= 0x3000 && characterCode <= 0x303F) ||
            (characterCode >= 0xFF00 && characterCode <= 0xFFEF);

        if (isCJKPunct) {
            drawX += readerFontSize / 4;
        }

        readerFontDraw(utf8Buf, drawX, drawY);
        cursorX += charW;
    }

    uint32_t nextPageStartByte = currentFileIdx;
    if (currentPageNum + 1 == (int)pageOffsets.size() && nextPageStartByte < bookDataSize) {
        pageOffsets.push_back(nextPageStartByte);
        totalPages = pageOffsets.size();
    }

    // Footer (book title)
    String footerName = currentBookName;
    footerName.replace(".txt", "");
    footerName.replace(".epub", "");
    M5.Display.setFont(&fonts::efontTW_16_b);
    // M5.Display.setTextSize(0.75f);
    M5.Display.drawString(footerName, 20, 760);
    // M5.Display.setTextSize(1.0f);

    // Footer: percentage through the book (or "End" if finished)
    uint32_t offset = pageOffsets[currentPageNum];
    uint32_t total  = (uint32_t)bookDataSize;
    int percent = (total > 0) ? (int)((uint64_t)offset * 100 / total) : 0;

    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    String footerMsg;
    if (nextPageStartByte >= bookDataSize) {
        footerMsg = "End";
    } else {
        footerMsg = String(percent) + "%";
    }

    int rightMargin = 20;                       // right edge distance from screen edge
    int msgW = M5.Display.textWidth(footerMsg);
    int msgX = 480 - rightMargin - msgW;        // left x so right edge is at 460
    M5.Display.drawString(footerMsg, msgX, FOOTER_Y + 10);

    endDisplay();

    // Save position only if we haven't reached the end of the book
    if (nextPageStartByte < bookDataSize) {
        saveResumePosition(false);
    } else {
        Serial.println("[Resume] at end of book — not saving");
    }

    Serial.println("[Reader] drawPage done");
    Serial.flush();
}

// ============================================================
// Book browser
// ============================================================
void drawBrowser() {
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    beginDisplay();
    clearScreen();

    drawHeaderReader("");
    if (books.size() == 0) {
        M5.Display.setFont(&fonts::FreeSans12pt7b);
        M5.Display.drawString("No books found.", 140, 350);
        M5.Display.drawString("Put " + ebookSupportedExtensions() + " in /books", 140, 400);
    } else {
        int visible = min(15, (int)books.size() - readerScroll);
        for (int i = 0; i < visible; i++) {
            int idx = readerScroll + i;
            int y = 90 + i * 45;
            String name = books[idx].substring(books[idx].lastIndexOf('/') + 1);
            name.replace(".txt", "");
            name.replace(".epub", "");
            bool selected = (idx == readerSelection);

            if (selected) {
                M5.Display.fillRect(10, y - 2, 460, 40, TFT_BLACK);
            }
            M5.Display.setFont(&fonts::efontTW_24_b);
            M5.Display.setTextColor(selected ? TFT_WHITE : TFT_BLACK,
                                    selected ? TFT_BLACK : TFT_WHITE);
            M5.Display.drawString(name, 20, y + 4);
        }
    }
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

    // Version
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString("READER v1.0", 20, 760);

    endDisplay();
}

void drawBrowserRow(int idx) {
    if (idx < 0 || idx >= (int)books.size()) return;

    int rowInView = idx - readerScroll;
    if (rowInView < 0 || rowInView >= 15) return;

    int y = 90 + rowInView * 45;
    String name = books[idx].substring(books[idx].lastIndexOf('/') + 1);
    name.replace(".txt", "");
    name.replace(".epub", "");
    bool selected = (idx == readerSelection);

    M5.Display.fillRect(0, y - 2, 480, 44, TFT_WHITE);
    if (selected) {
        M5.Display.fillRect(10, y - 2, 460, 40, TFT_BLACK);
    }

    M5.Display.setFont(&fonts::efontTW_24_b);
    M5.Display.setTextColor(selected ? TFT_WHITE : TFT_BLACK,
                            selected ? TFT_BLACK : TFT_WHITE);
    M5.Display.drawString(name, 20, y + 4);
}

static int browserFastRefreshCount = 0;
static void maybeGhostClearBrowser() {
    if (++browserFastRefreshCount >= 20) {
        browserFastRefreshCount = 0;
        drawBrowser();
    }
}

static void listMoveUp() {
    if (books.empty()) return;
    if (millis() - lastScrollTime < SCROLL_DEBOUNCE_MS) return;
    lastScrollTime = millis();

    if (readerSelection <= 0) return;

    int oldSel = readerSelection;
    readerSelection--;

    if (readerSelection < readerScroll) {
        readerScroll = readerSelection;
        drawBrowser();
        return;
    }

    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    beginDisplay();
    drawBrowserRow(oldSel);
    drawBrowserRow(readerSelection);
    endDisplay();
    maybeGhostClearBrowser();
}

static void listMoveDown() {
    if (books.empty()) return;
    if (millis() - lastScrollTime < SCROLL_DEBOUNCE_MS) return;
    lastScrollTime = millis();

    if (readerSelection >= (int)books.size() - 1) return;

    int oldSel = readerSelection;
    readerSelection++;

    const int VISIBLE_ROWS = 15;
    if (readerSelection >= readerScroll + VISIBLE_ROWS) {
        readerScroll = readerSelection - VISIBLE_ROWS + 1;
        drawBrowser();
        return;
    }

    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    beginDisplay();
    drawBrowserRow(oldSel);
    drawBrowserRow(readerSelection);
    endDisplay();
    maybeGhostClearBrowser();
}

static void listOpenSelected() {
    if (books.empty()) return;
    openBook(readerSelection);
}

static void pagePrev() {
    if (currentPageNum > 0) {
        currentPageNum--;
        drawPage();
    }
}

static void pageNext() {
    if (currentPageNum + 1 < (int)pageOffsets.size()) {
        currentPageNum++;
        drawPage();
        return;
    }
    if (pageOffsets.empty()) return;
    if (pageOffsets.back() < bookDataSize) {
        currentPageNum++;
        drawPage();
    }
}

// ============================================================
// Touch gesture polling
// ============================================================
static RdGesture pollReaderGesture(int& tapX, int& tapY) {
    RdGesture result = RD_GESTURE_NONE;
    if (M5.Touch.getCount() > 0) {
        auto detail = M5.Touch.getDetail(0);
        if (detail.isPressed()) {
            int mx, my;
            mapTouchToDisplay(detail.x, detail.y, mx, my);
            if (!rdTouchActive) {
                rdTouchActive    = true;
                rdTouchStartX    = mx;
                rdTouchStartY    = my;
                rdTouchStartTime = millis();
                rdSwipeHandled   = false;
            }
        } else if (rdTouchActive) {
            if (!rdSwipeHandled) {
                int mx, my;
                mapTouchToDisplay(detail.x, detail.y, mx, my);
                int dx = mx - rdTouchStartX;
                int dy = my - rdTouchStartY;
                unsigned long dt = millis() - rdTouchStartTime;
                int adx = abs(dx);
                int ady = abs(dy);

                if (dt < RD_SWIPE_MAX_MS && (adx > RD_SWIPE_MIN_DIST || ady > RD_SWIPE_MIN_DIST)) {
                    if (adx > ady * RD_SWIPE_DOMINANCE) {
                        result = (dx > 0) ? RD_GESTURE_SWIPE_RIGHT : RD_GESTURE_SWIPE_LEFT;
                    } else if (ady > adx * RD_SWIPE_DOMINANCE) {
                        result = (dy > 0) ? RD_GESTURE_SWIPE_DOWN : RD_GESTURE_SWIPE_UP;
                    } else {
                        tapX = mx; tapY = my; result = RD_GESTURE_TAP;
                    }
                } else {
                    // It's a tap. Check the info button first so it wins
                    // over any other tap handler.
                    if (hitBackButton(mx, my)) {
                        result = RD_GESTURE_BACK;
                    } else if (hitInfoButton(mx, my)) {
                        result = RD_GESTURE_INFO;
                    } else {
                        tapX = mx; tapY = my;
                        result = RD_GESTURE_TAP;
                    }
                }
                rdSwipeHandled = true;
            }
            rdTouchActive = false;
            resetTouch();
        }
    } else {
        rdTouchActive = false;
    }
    return result;
}

// ============================================================
// Reader preference persistence
// ============================================================
void saveReaderPrefs() {
    File f;
    if (isSdMounted()) {
        f = SD_MMC.open(READER_PREFS_PATH, FILE_WRITE);
    } else {
        f = LittleFS.open(READER_PREFS_PATH, FILE_WRITE);
    }
    if (!f) return;
    f.printf("fontLevel=%d\n", readerFontLevel);
    f.close();
}

void loadReaderPrefs() {
    File f;
    if (isSdMounted() && SD_MMC.exists(READER_PREFS_PATH)) {
        f = SD_MMC.open(READER_PREFS_PATH, FILE_READ);
    } else if (LittleFS.exists(READER_PREFS_PATH)) {
        f = LittleFS.open(READER_PREFS_PATH, FILE_READ);
    }
    if (!f) {
        applyReaderFont();
        return;
    }
    String s = f.readString();
    f.close();

    int lineStart = 0;
    while (lineStart < s.length()) {
        int lineEnd = s.indexOf('\n', lineStart);
        if (lineEnd < 0) lineEnd = s.length();
        String line = s.substring(lineStart, lineEnd);
        line.trim();
        if (line.startsWith("fontLevel=")) {
            int lvl = line.substring(10).toInt();
            if (lvl >= 0 && lvl < kReaderFontCount) readerFontLevel = lvl;
        }
        lineStart = lineEnd + 1;
    }
    applyReaderFont();
}

// ============================================================
// Main reader loop
// ============================================================
void readerLoop() {
    static bool prefsLoaded = false;
    if (!prefsLoaded) {
        loadReaderPrefs();
        prefsLoaded = true;
    }

    checkBookLoadComplete();

    if (bookLoadInProgress) {
        delay(50);
        return;
    }

    if (M5.BtnA.wasHold()) {
        if (confirmDialog("Quit?", "Return to the launcher?", "Quit", "Stay")) {
            if (readerView == "read") saveResumePosition(true);
            if (bookData) { heap_caps_free(bookData); bookData = nullptr; bookDataSize = 0; }
            readerFontUnload();
            enterLauncher();
            delay(200);
        } else {
            drawPage();      // or drawBrowser() if readerView == "list"
        }
        return;
    }

    if (M5.BtnB.wasHold()) {
        // Only ask when in reading view; book list is already a small step.
        if (readerView == "read") {
            if (confirmDialog("Book list?", "Return to the book list?", "Yes", "No")) {
                readerView = "list";
                drawBrowser();
            } else {
                drawPage();
            }
        } else {
            listOpenSelected();
        }
        return;
    }

    int tapX = 0, tapY = 0;
    RdGesture g = pollReaderGesture(tapX, tapY);

    // ---- Back button ----
    // In the reader, back means different things in each view:
    //   reading view  -> book list
    //   book list     -> launcher
    if (g == RD_GESTURE_BACK) {
        if (readerView == "read") {
            saveResumePosition(true);
            readerView = "list";
            drawBrowser();
        } else {
            // Book list (or any non-reading view) -> launcher
            if (bookData) { heap_caps_free(bookData); bookData = nullptr; bookDataSize = 0; }
            readerFontUnload();
            enterLauncher();
        }
        rdTouchActive  = false;
        rdSwipeHandled = true;
        return;
    }

    // ---- Info button ----
    // Handled as its own gesture so it never collides with page/list logic.
    if (g == RD_GESTURE_INFO) {
        if (readerView == "list") {
            static const char* const readerListHelp[] = {
                "Tap a book = Open it",
                "BtnA / BtnB (short) = Move selection",
                "Swipe Up / Down = Scroll list",
                "",
                "Hold BtnA = Launcher",
                "Hold BtnB = Open selected book",
                "Hold BtnA + BtnB = Sleep / Wake",
            };
            showInfoModalAndWait("READER v.10", readerListHelp, 7);
            rdTouchActive  = false;
            rdSwipeHandled = true;
            drawBrowser();
        } else {
            static const char* const readerReadingHelp[] = {
                "Swipe L / R = Page back / forward",
                "Swipe U / D = Font size up / down",
                "BtnA / BtnB = Page back / forward",
                "",
                "Hold BtnA = Launcher",
                "Hold BtnB = Book list",
                "Hold BtnA + BtnB = Sleep / Wake",
            };
            showInfoModalAndWait("READER v1.0", readerReadingHelp, 7);
            rdTouchActive  = false;
            rdSwipeHandled = true;
            drawPage();
        }
        return;
    }

    // ---- List view ----
    if (readerView == "list") {
        if (M5.BtnA.wasClicked()) { listMoveUp();   return; }
        if (M5.BtnB.wasClicked()) { listMoveDown(); return; }

        switch (g) {
            case RD_GESTURE_SWIPE_UP:   listMoveUp();   return;
            case RD_GESTURE_SWIPE_DOWN: listMoveDown(); return;
            case RD_GESTURE_TAP: {
                if (tapY >= 750) { enterLauncher(); return; }
                if (tapY >= 90 && tapY < 745) {
                    int rowTapped = (tapY - 90) / 45;
                    int idx = rowTapped + readerScroll;
                    if (idx >= 0 && idx < (int)books.size()) {
                        readerSelection = idx;
                        openBook(idx);
                    }
                }
                return;
            }
            default: break;
        }
    }

    // ---- Reading view ----
    if (readerView == "read") {
        if (M5.BtnA.wasClicked()) { pagePrev(); return; }
        if (M5.BtnB.wasClicked()) { pageNext(); return; }

        switch (g) {
            case RD_GESTURE_SWIPE_LEFT:  pageNext(); return;
            case RD_GESTURE_SWIPE_RIGHT: pagePrev(); return;
            case RD_GESTURE_SWIPE_UP:    changeReaderFontSize(+1); return;
            case RD_GESTURE_SWIPE_DOWN:  changeReaderFontSize(-1); return;
            case RD_GESTURE_TAP: {
                // if (tapY < 45) { readerView = "list"; drawBrowser(); return; }
                if (tapY >= 90 && tapY < 745) {
                    if (tapX < 160) { pagePrev(); }
                    else { pageNext(); }
                    return;
                }
            }
            default: break;
        }
    }
}
