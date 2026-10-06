#ifndef EBOOK_DECODE_H
#define EBOOK_DECODE_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include <string>
#include <vector>

// ============================================================
// Format enable/disable switches
// ============================================================
#define EBOOK_ENABLE_TXT   0
#define EBOOK_ENABLE_EPUB  1

// ============================================================
// Structured result
// ============================================================
struct EbookChapter {
    String   title;        // may be empty
    String   href;         // may be empty for TXT
    uint32_t textOffset;   // byte offset into the flat text buffer
    uint32_t textLength;   // bytes of text in this chapter (0 if unknown)
};

struct EbookTocEntry {
    String   title;
    int      chapterIndex; // -1 if it doesn't map to a spine item
    uint32_t textOffset;   // resolved offset into the flat buffer
};

struct EbookDoc {
    uint8_t*  text = nullptr;   // PSRAM buffer, caller frees via ebookFreeDoc()
    size_t    textSize = 0;
    std::vector<EbookChapter>  chapters;
    std::vector<EbookTocEntry> toc;
    String    title;            // book title if discoverable
};

// ============================================================
// Public API
// ============================================================

// Structured load. On success fills `doc` and returns true. Caller must
// call ebookFreeDoc() when done.
bool ebookLoadDoc(const String& path, EbookDoc& doc);

// Legacy flat load, kept for compatibility with existing call sites.
// Wraps ebookLoadDoc and hands out only the text buffer.
bool ebookLoad(const String& path, uint8_t** outData, size_t* outSize);

// Free the text buffer owned by `doc` and clear it.
void ebookFreeDoc(EbookDoc& doc);

// True if the file extension is supported by a currently-enabled backend.
bool ebookIsSupported(const String& path);

// The set of extensions the dispatcher will accept, comma-separated.
String ebookSupportedExtensions();

#endif
