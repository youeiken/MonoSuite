#include "ebook_decode.h"

#if EBOOK_ENABLE_TXT
#include "ebook_txt.h"
#endif
#if EBOOK_ENABLE_EPUB
#include "ebook_epub.h"
#endif

static String lowerExt(const String& path) {
    int dot = path.lastIndexOf('.');
    if (dot < 0) return "";
    String ext = path.substring(dot + 1);
    ext.toLowerCase();
    return ext;
}

bool ebookIsSupported(const String& path) {
    String ext = lowerExt(path);
#if EBOOK_ENABLE_TXT
    if (ext == "txt" || ext == "md") return true;
#endif
#if EBOOK_ENABLE_EPUB
    if (ext == "epub") return true;
#endif
    return false;
}

String ebookSupportedExtensions() {
    String s;
#if EBOOK_ENABLE_TXT
    s += ".txt .md ";
#endif
#if EBOOK_ENABLE_EPUB
    s += ".epub ";
#endif
    s.trim();
    return s;
}

void ebookFreeDoc(EbookDoc& doc) {
    if (doc.text) {
        heap_caps_free(doc.text);
        doc.text = nullptr;
    }
    doc.textSize = 0;
    doc.chapters.clear();
    doc.toc.clear();
    doc.title = "";
}

bool ebookLoadDoc(const String& path, EbookDoc& doc) {
    ebookFreeDoc(doc);

    String ext = lowerExt(path);

#if EBOOK_ENABLE_TXT
    if (ext == "txt" || ext == "md") {
        return ebookTxtLoadDoc(path, doc);
    }
#endif

#if EBOOK_ENABLE_EPUB
    if (ext == "epub") {
        return ebookEpubLoadDoc(path, doc);
    }
#endif

    Serial.printf("[Ebook] Unsupported extension: %s\n", ext.c_str());
    return false;
}

bool ebookLoad(const String& path, uint8_t** outData, size_t* outSize) {
    *outData = nullptr;
    *outSize = 0;

    EbookDoc doc;
    if (!ebookLoadDoc(path, doc)) return false;

    // Hand ownership of the text buffer to the caller.
    *outData = doc.text;
    *outSize = doc.textSize;
    doc.text = nullptr;   // prevent double free
    doc.textSize = 0;
    return true;
}
