#include "ebook_txt.h"

#include <LittleFS.h>
#include <SD_MMC.h>

static bool openReadableFile(const String& path, File& out) {
    return storageOpenRead(path.c_str(), out);
}

bool ebookTxtLoadDoc(const String& path, EbookDoc& doc) {
    ebookFreeDoc(doc);

    File f;
    if (!openReadableFile(path, f)) return false;

    size_t sz = f.size();
    if (sz == 0) { f.close(); return false; }

    uint8_t* buf = (uint8_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
    if (!buf) {
        Serial.printf("[Txt] PSRAM alloc failed (%u bytes)\n", (unsigned)sz);
        f.close();
        return false;
    }

    size_t total = 0;
    while (total < sz) {
        size_t chunk = sz - total;
        if (chunk > 8192) chunk = 8192;
        size_t got = f.read(buf + total, chunk);
        if (got == 0) {
            Serial.printf("[Txt] read stalled at %u / %u\n",
                          (unsigned)total, (unsigned)sz);
            heap_caps_free(buf);
            f.close();
            return false;
        }
        total += got;
    }
    f.close();

    doc.text = buf;
    doc.textSize = total;

    EbookChapter c;
    c.textOffset = 0;
    c.textLength = total;
    c.title = "";
    c.href  = "";
    doc.chapters.push_back(c);

    EbookTocEntry t;
    t.title = "Start";
    t.chapterIndex = 0;
    t.textOffset = 0;
    doc.toc.push_back(t);

    Serial.printf("[Txt] loaded %u bytes\n", (unsigned)total);
    return true;
}
