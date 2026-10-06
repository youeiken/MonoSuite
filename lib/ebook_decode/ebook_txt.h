#ifndef EBOOK_TXT_H
#define EBOOK_TXT_H

#include "ebook_decode.h"
#include "storage.h"
#include <Arduino.h>
#include <string>

bool ebookTxtLoadDoc(const String& path, EbookDoc& doc);

#endif
