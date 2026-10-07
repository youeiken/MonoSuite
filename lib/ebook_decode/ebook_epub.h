#ifndef EBOOK_EPUB_H
#define EBOOK_EPUB_H

#include "ebook_decode.h"
#include "storage.h"
#include <Arduino.h>
#include <WString.h>

// Structured EPUB load. Fills doc.chapters, doc.toc, doc.title.
bool ebookEpubLoadDoc(const String& path, EbookDoc& doc);

#endif
