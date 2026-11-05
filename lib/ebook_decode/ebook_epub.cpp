#include "ebook_epub.h"

#include "miniz_epub.h"
#include <algorithm>
#include <cstdlib>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <vector>

// ============================================================
// Small XML helpers
// ============================================================

static bool startsWithCI(const char* s, const char* prefix) {
    while (*prefix) {
        char a = *s++, b = *prefix++;
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
    }
    return true;
}

static String getAttr(const String& tag, const char* attrName) {
    String pattern = String(attrName) + "=";
    int p = 0;
    while ((p = tag.indexOf(pattern, p)) >= 0) {
        if (p == 0 || tag[p - 1] == ' ' || tag[p - 1] == '\t' ||
            tag[p - 1] == '\n' || tag[p - 1] == '\r' || tag[p - 1] == '<') {
            int q = p + pattern.length();
            if (q < (int)tag.length() && (tag[q] == '"' || tag[q] == '\'')) {
                char quote = tag[q];
                int end = tag.indexOf(quote, q + 1);
                if (end > q) return tag.substring(q + 1, end);
            }
        }
        p += pattern.length();
    }
    return "";
}

static String xmlUnescape(const String& s) {
    String out = s;
    out.replace("&amp;", "&");
    out.replace("&lt;", "<");
    out.replace("&gt;", ">");
    out.replace("&quot;", "\"");
    out.replace("&apos;", "'");
    out.replace("&#39;", "'");
    return out;
}

// ============================================================
// OPF parsing
// ============================================================

struct ManifestItem {
    String id;
    String href;
    String mediaType;
    String properties;
};

struct SpineItem {
    String idref;
    bool linear = true;
};

static String extractOpfPath(const uint8_t* xml, size_t len) {
    String s;
    s.reserve(len);
    for (size_t i = 0; i < len; i++) s += (char)xml[i];

    int pos = 0;
    while ((pos = s.indexOf("full-path", pos)) >= 0) {
        int lt = s.lastIndexOf('<', pos);
        int gt = s.indexOf('>', pos);
        if (lt >= 0 && gt > lt) {
            String tag = s.substring(lt, gt + 1);
            String val = getAttr(tag, "full-path");
            if (val.length() > 0) return val;
        }
        pos += 9;
    }
    return "";
}

static String extractOpfTitle(const String& opf) {
    int a = opf.indexOf("<dc:title");
    if (a < 0) a = opf.indexOf("<title");
    if (a < 0) return "";
    int gt = opf.indexOf('>', a);
    if (gt < 0) return "";
    int end = opf.indexOf('<', gt + 1);
    if (end < 0) return "";
    String t = opf.substring(gt + 1, end);
    t.trim();
    return xmlUnescape(t);
}

static void parseManifest(const String& opf, std::vector<ManifestItem>& out) {
    int pos = 0;
    while ((pos = opf.indexOf("<item ", pos)) >= 0) {
        int gt = opf.indexOf('>', pos);
        if (gt < 0) break;
        String tag = opf.substring(pos, gt + 1);
        pos = gt + 1;

        ManifestItem it;
        it.id          = getAttr(tag, "id");
        it.href        = getAttr(tag, "href");
        it.mediaType   = getAttr(tag, "media-type");
        it.properties  = getAttr(tag, "properties");
        if (it.id.length() && it.href.length()) {
            out.push_back(it);
        }
    }
}

static void parseSpine(const String& opf, std::vector<SpineItem>& out) {
    int spineStart = opf.indexOf("<spine");
    if (spineStart < 0) return;
    int spineEnd = opf.indexOf("</spine>", spineStart);
    if (spineEnd < 0) spineEnd = opf.length();

    int pos = spineStart;
    while ((pos = opf.indexOf("<itemref ", pos)) >= 0 && pos < spineEnd) {
        int gt = opf.indexOf('>', pos);
        if (gt < 0) break;
        String tag = opf.substring(pos, gt + 1);
        pos = gt + 1;

        SpineItem si;
        si.idref = getAttr(tag, "idref");
        String lin = getAttr(tag, "linear");
        lin.toLowerCase();
        si.linear = (lin != "no");
        if (si.idref.length()) out.push_back(si);
    }
}

// ============================================================
// XHTML -> plain text
// ============================================================

static void stripXhtml(const uint8_t* xml, size_t len, String& out) {
    size_t i = 0;
    bool inHead = false;
    bool inSkip = false;
    bool inTag = false;
    bool closingTag = false;
    char tagBuf[64];
    int tagLen = 0;

    if (len >= 3 && xml[0] == 0xEF && xml[1] == 0xBB && xml[2] == 0xBF) {
        i = 3;
    }

    while (i < len) {
        uint8_t c = xml[i];

        if (inTag) {
            if (c == '>') {
                inTag = false;
                tagBuf[tagLen] = 0;

                bool isBr = startsWithCI(tagBuf, "br");
                bool isP  = (tagBuf[0] == 'p' || tagBuf[0] == 'P') &&
                            (tagBuf[1] == 0 || tagBuf[1] == ' ' || tagBuf[1] == '/');
                bool isDiv = startsWithCI(tagBuf, "div");
                bool isH1 = startsWithCI(tagBuf, "h1");
                bool isH2 = startsWithCI(tagBuf, "h2");
                bool isH3 = startsWithCI(tagBuf, "h3");
                bool isH4 = startsWithCI(tagBuf, "h4");
                bool isH5 = startsWithCI(tagBuf, "h5");
                bool isH6 = startsWithCI(tagBuf, "h6");
                bool isHead = startsWithCI(tagBuf, "head");
                bool isStyle = startsWithCI(tagBuf, "style");
                bool isScript = startsWithCI(tagBuf, "script");

                if (isHead) inHead = !closingTag;
                else if (isStyle || isScript) inSkip = !closingTag;
                else if (!inHead && !inSkip) {
                    if (isBr || isP || isDiv ||
                        isH1 || isH2 || isH3 || isH4 || isH5 || isH6) {
                        out += '\n';
                    }
                }

                i++;
                continue;
            }

            if (tagLen < (int)sizeof(tagBuf) - 1) {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '/' || c == ' ' ||
                    c == '=' || c == '"' || c == '\'' ||
                    c == '-' || c == '_' || c == '.' || c == ':') {
                    tagBuf[tagLen++] = (char)c;
                }
            }
            i++;
            continue;
        }

        if (c == '<') {
            if (i + 3 < len && xml[i+1] == '!' && xml[i+2] == '-' && xml[i+3] == '-') {
                i += 4;
                while (i + 2 < len &&
                       !(xml[i] == '-' && xml[i+1] == '-' && xml[i+2] == '>')) {
                    i++;
                }
                i += 3;
                continue;
            }

            inTag = true;
            closingTag = false;
            tagLen = 0;
            i++;
            if (i < len && xml[i] == '/') { closingTag = true; i++; }
            continue;
        }

        if (inHead || inSkip) { i++; continue; }

        if (c == '&') {
            size_t semi = i + 1;
            while (semi < len && semi < i + 11 && xml[semi] != ';') semi++;
            if (semi < len && xml[semi] == ';') {
                String entity;
                for (size_t j = i + 1; j < semi; j++) entity += (char)xml[j];

                uint32_t cp = 0;
                bool known = true;
                if (entity == "amp")        cp = '&';
                else if (entity == "lt")    cp = '<';
                else if (entity == "gt")    cp = '>';
                else if (entity == "quot")  cp = '"';
                else if (entity == "apos")  cp = '\'';
                else if (entity == "nbsp")  cp = 0x00A0;
                else if (entity == "mdash") cp = 0x2014;
                else if (entity == "ndash") cp = 0x2013;
                else if (entity == "hellip") cp = 0x2026;
                else if (entity == "ldquo") cp = 0x201C;
                else if (entity == "rdquo") cp = 0x201D;
                else if (entity == "lsquo") cp = 0x2018;
                else if (entity == "rsquo") cp = 0x2019;
                else if (entity.length() > 1 && entity[0] == '#') {
                    if (entity[1] == 'x' || entity[1] == 'X') {
                        cp = (uint32_t)strtoul(entity.c_str() + 2, nullptr, 16);
                    } else {
                        cp = (uint32_t)strtoul(entity.c_str() + 1, nullptr, 10);
                    }
                } else {
                    known = false;
                }

                if (known) {
                    if (cp < 0x80) {
                        out += (char)cp;
                    } else if (cp < 0x800) {
                        out += (char)(0xC0 | (cp >> 6));
                        out += (char)(0x80 | (cp & 0x3F));
                    } else if (cp < 0x10000) {
                        out += (char)(0xE0 | (cp >> 12));
                        out += (char)(0x80 | ((cp >> 6) & 0x3F));
                        out += (char)(0x80 | (cp & 0x3F));
                    }
                    i = semi + 1;
                    continue;
                }
            }
            out += '&';
            i++;
            continue;
        }

        out += (char)c;
        i++;
    }
}

// ============================================================
// TOC parsing (NCX + nav.xhtml)
// ============================================================

struct RawTocEntry {
    String title;
    String href;   // may include #fragment
};

static void parseNcx(const uint8_t* xml, size_t len,
                     std::vector<RawTocEntry>& out) {
    String s;
    s.reserve(len);
    for (size_t i = 0; i < len; i++) s += (char)xml[i];

    int pos = 0;
    while ((pos = s.indexOf("<navPoint", pos)) >= 0) {
        int navEnd = s.indexOf("</navPoint>", pos);
        if (navEnd < 0) navEnd = s.length();

        RawTocEntry e;

        int tStart = s.indexOf("<text", pos);
        if (tStart >= 0 && tStart < navEnd) {
            int gt = s.indexOf('>', tStart);
            int lt = s.indexOf("</text>", gt + 1);
            if (gt > 0 && lt > gt) {
                e.title = s.substring(gt + 1, lt);
                e.title.trim();
                e.title = xmlUnescape(e.title);
            }
        }

        int cStart = s.indexOf("<content", pos);
        if (cStart >= 0 && cStart < navEnd) {
            int gt = s.indexOf('>', cStart);
            if (gt > 0) {
                String tag = s.substring(cStart, gt + 1);
                e.href = getAttr(tag, "src");
            }
        }

        if (e.href.length()) out.push_back(e);

        pos = navEnd + 1;
    }
}

static void parseNavXhtml(const uint8_t* xml, size_t len,
                          std::vector<RawTocEntry>& out) {
    String s;
    s.reserve(len);
    for (size_t i = 0; i < len; i++) s += (char)xml[i];

    // Locate the TOC nav element.
    int navStart = s.indexOf("epub:type=\"toc\"");
    if (navStart < 0) navStart = s.indexOf("epub:type='toc'");
    if (navStart < 0) {
        navStart = s.indexOf("role=\"doc-toc\"");
        if (navStart < 0) navStart = 0;
    }

    // Bound the scan to the </nav> that closes this element, so we
    // don't pick up <a> tags from landmarks or page-list sections.
    int navEnd = s.indexOf("</nav>", navStart);
    if (navEnd < 0) navEnd = s.length();

    int pos = navStart;
    while ((pos = s.indexOf("<a ", pos)) >= 0 && pos < navEnd) {
        int gt = s.indexOf('>', pos);
        if (gt < 0 || gt >= navEnd) break;
        String tag = s.substring(pos, gt + 1);
        int lt = s.indexOf("</a>", gt + 1);
        if (lt < 0 || lt >= navEnd) break;

        RawTocEntry e;
        e.href = getAttr(tag, "href");
        e.title = s.substring(gt + 1, lt);
        e.title.trim();
        e.title = xmlUnescape(e.title);

        if (e.href.length() && e.title.length()) out.push_back(e);
        pos = lt + 3;
    }
}

// ============================================================
// File helpers
// ============================================================

static bool openReadableFile(const String& path, File& out) {
    return storageOpenRead(path.c_str(), out);
}

// ============================================================
// Path normalization
// ============================================================

// Resolves "." and ".." segments in a ZIP-style path. Leading
// slash is stripped. Backslashes are treated as separators too,
// since some malformed EPUBs use them.
//
//   normalizeZipPath("OEBPS/../Text/a.xhtml") -> "Text/a.xhtml"
//   normalizeZipPath("./a.xhtml")             -> "a.xhtml"
//   normalizeZipPath("a//b/./c")              -> "a/b/c"
//   normalizeZipPath("../a")                  -> "a"   (leading .. dropped)
static String normalizeZipPath(const String& in) {
    String s = in;
    s.replace('\\', '/');
    while (s.startsWith("/")) s = s.substring(1);

    std::vector<String> stack;
    int start = 0;
    while (start <= (int)s.length()) {
        int slash = s.indexOf('/', start);
        if (slash < 0) slash = s.length();
        String seg = s.substring(start, slash);
        start = slash + 1;

        if (seg.length() == 0 || seg == ".") continue;
        if (seg == "..") {
            if (!stack.empty()) stack.pop_back();
            continue;
        }
        stack.push_back(seg);
    }

    String out;
    for (size_t i = 0; i < stack.size(); i++) {
        if (i > 0) out += '/';
        out += stack[i];
    }
    return out;
}

// Normalize a possibly-relative href against a base dir.
// Returns a clean ZIP entry name (no leading slash, no "." / "..").
static String resolveHref(const String& baseDir, const String& href) {
    String h = href;
    int hash = h.indexOf('#');
    if (hash >= 0) h = h.substring(0, hash);
    h.trim();
    if (h.length() == 0) return "";

    if (h.startsWith("/")) return normalizeZipPath(h);

    String combined = baseDir;
    if (combined.length() > 0 && !combined.endsWith("/")) combined += '/';
    combined += h;
    return normalizeZipPath(combined);
}

// Directory portion of a normalized path, with trailing slash.
// Returns "" if the path has no directory component.
static String dirOf(const String& path) {
    int slash = path.lastIndexOf('/');
    if (slash < 0) return "";
    return path.substring(0, slash + 1);
}

// ============================================================
// Public entry point
// ============================================================
bool ebookEpubLoadDoc(const String& path, EbookDoc& doc) {
    File f;
    if (!openReadableFile(path, f)) return false;
    size_t epubSize = f.size();
    if (epubSize == 0) { f.close(); return false; }

    uint8_t* epubBuf = (uint8_t*)heap_caps_malloc(epubSize, MALLOC_CAP_SPIRAM);
    if (!epubBuf) { f.close(); return false; }

    size_t total = 0;
    while (total < epubSize) {
        size_t chunk = epubSize - total;
        if (chunk > 8192) chunk = 8192;
        size_t got = f.read(epubBuf + total, chunk);
        if (got == 0) break;
        total += got;
    }
    f.close();
    if (total != epubSize) { heap_caps_free(epubBuf); return false; }

    Serial.printf("[EPUB] read %u bytes\n", (unsigned)epubSize);

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_mem(&zip, epubBuf, epubSize, 0)) {
        Serial.println("[EPUB] zip init failed");
        heap_caps_free(epubBuf);
        return false;
    }

    // ---- container.xml -> OPF path
    size_t cs = 0;
    uint8_t* cont = (uint8_t*)mz_zip_reader_extract_file_to_heap(
        &zip, "META-INF/container.xml", &cs, 0);
    if (!cont) {
        Serial.println("[EPUB] container.xml missing");
        mz_zip_reader_end(&zip);
        heap_caps_free(epubBuf);
        return false;
    }
    String opfPath = normalizeZipPath(extractOpfPath(cont, cs));
    mz_free(cont);
    Serial.printf("[EPUB] OPF path = %s\n", opfPath.c_str());
    if (opfPath.length() == 0) {
        mz_zip_reader_end(&zip);
        heap_caps_free(epubBuf);
        return false;
    }

    // ---- OPF
    size_t os = 0;
    uint8_t* opfBytes = (uint8_t*)mz_zip_reader_extract_file_to_heap(
        &zip, opfPath.c_str(), &os, 0);
    if (!opfBytes) {
        Serial.println("[EPUB] OPF extract failed");
        mz_zip_reader_end(&zip);
        heap_caps_free(epubBuf);
        return false;
    }

    String opf;
    opf.reserve(os);
    for (size_t i = 0; i < os; i++) opf += (char)opfBytes[i];
    mz_free(opfBytes);

    doc.title = extractOpfTitle(opf);

    std::vector<ManifestItem> manifest;
    std::vector<SpineItem>    spine;
    parseManifest(opf, manifest);
    parseSpine(opf, spine);

    if (spine.empty()) {
        Serial.println("[EPUB] empty spine");
        mz_zip_reader_end(&zip);
        heap_caps_free(epubBuf);
        return false;
    }

    String opfDir = dirOf(opfPath);

    // Map id -> manifest item for quick lookup.
    auto findItem = [&](const String& id) -> const ManifestItem* {
        for (const auto& it : manifest) if (it.id == id) return &it;
        return nullptr;
    };

    // ---- Extract text per spine item, record chapter offsets
    String text;
    text.reserve(512 * 1024);

    for (const auto& sp : spine) {
        const ManifestItem* mi = findItem(sp.idref);
        if (!mi) continue;
        if (mi->mediaType.indexOf("html") < 0) continue;

        String full = resolveHref(opfDir, mi->href);
        if (full.length() == 0) continue;

        size_t xs = 0;
        uint8_t* chap = (uint8_t*)mz_zip_reader_extract_file_to_heap(
            &zip, full.c_str(), &xs, 0);
        if (!chap) {
            Serial.printf("[EPUB]   MISSING: %s\n", full.c_str());
            continue;
        }

        EbookChapter ec;
        ec.href = mi->href;             // keep the raw manifest href
        ec.textOffset = text.length();

        // Sniff a <title> from the first 2 KB for use as a fallback.
        String chapterTitle;
        {
            size_t scan = xs < 2048 ? xs : 2048;
            String head;
            head.reserve(scan);
            for (size_t i = 0; i < scan; i++) head += (char)chap[i];
            int ts = head.indexOf("<title");
            if (ts >= 0) {
                int gt = head.indexOf('>', ts);
                int te = head.indexOf("</title>", gt + 1);
                if (gt > 0 && te > gt) {
                    chapterTitle = head.substring(gt + 1, te);
                    chapterTitle.trim();
                    chapterTitle = xmlUnescape(chapterTitle);
                }
            }
        }
        ec.title = chapterTitle;

        stripXhtml(chap, xs, text);
        text += "\n\n";
        mz_free(chap);

        ec.textLength = text.length() - ec.textOffset;
        doc.chapters.push_back(ec);
    }

    // ---- Locate TOC sources
    const ManifestItem* ncxItem = nullptr;
    const ManifestItem* navItem = nullptr;
    for (const auto& it : manifest) {
        if (it.mediaType.indexOf("dtbncx") >= 0 ||
            it.href.endsWith(".ncx")) {
            ncxItem = &it;
        }
        if (it.properties.indexOf("nav") >= 0) {
            navItem = &it;
        }
    }

    // Parse the TOC, remembering which directory the entries are
    // relative to (NCX and nav.xhtml each have their own base).
    std::vector<RawTocEntry> rawToc;
    String tocBaseDir = opfDir;

    if (ncxItem) {
        String ncxPath = resolveHref(opfDir, ncxItem->href);
        tocBaseDir = dirOf(ncxPath);

        size_t ns = 0;
        uint8_t* ncx = (uint8_t*)mz_zip_reader_extract_file_to_heap(
            &zip, ncxPath.c_str(), &ns, 0);
        if (ncx) {
            parseNcx(ncx, ns, rawToc);
            mz_free(ncx);
            Serial.printf("[EPUB] NCX entries: %d (base=%s)\n",
                          (int)rawToc.size(), tocBaseDir.c_str());
        }
    }
    if (rawToc.empty() && navItem) {
        String navPath = resolveHref(opfDir, navItem->href);
        tocBaseDir = dirOf(navPath);

        size_t ns = 0;
        uint8_t* nav = (uint8_t*)mz_zip_reader_extract_file_to_heap(
            &zip, navPath.c_str(), &ns, 0);
        if (nav) {
            parseNavXhtml(nav, ns, rawToc);
            mz_free(nav);
            Serial.printf("[EPUB] nav.xhtml entries: %d (base=%s)\n",
                          (int)rawToc.size(), tocBaseDir.c_str());
        }
    }

    mz_zip_reader_end(&zip);
    heap_caps_free(epubBuf);

    if (text.length() == 0) {
        Serial.println("[EPUB] empty text");
        return false;
    }

    // Collapse runs of 3+ newlines down to 2.
    while (text.indexOf("\n\n\n") >= 0) text.replace("\n\n\n", "\n\n");

    // ---- Move text into PSRAM
    size_t n = text.length();
    uint8_t* out = (uint8_t*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    if (!out) {
        Serial.println("[EPUB] PSRAM alloc for text failed");
        return false;
    }
    memcpy(out, text.c_str(), n);

    doc.text = out;
    doc.textSize = n;

    // ---- Resolve TOC hrefs to chapter indices + text offsets.
    // Both sides are normalized to ZIP entry names so the comparison
    // is exact — no more fuzzy endsWith matching.
    for (const auto& raw : rawToc) {
        EbookTocEntry te;
        te.title = raw.title;
        te.chapterIndex = -1;
        te.textOffset = 0;

        String resolved = resolveHref(tocBaseDir, raw.href);

        for (size_t i = 0; i < doc.chapters.size(); i++) {
            String chapResolved = resolveHref(opfDir, doc.chapters[i].href);
            if (chapResolved == resolved) {
                te.chapterIndex = (int)i;
                te.textOffset = doc.chapters[i].textOffset;
                break;
            }
        }

        doc.toc.push_back(te);
    }

    // If we ended up with no TOC, synthesize one from chapters.
    if (doc.toc.empty()) {
        for (size_t i = 0; i < doc.chapters.size(); i++) {
            EbookTocEntry te;
            te.chapterIndex = (int)i;
            te.textOffset = doc.chapters[i].textOffset;
            te.title = doc.chapters[i].title;
            if (te.title.length() == 0) {
                te.title = "Chapter " + String((int)i + 1);
            }
            doc.toc.push_back(te);
        }
    }

    Serial.printf("[EPUB] loaded %u bytes, %d chapters, %d toc entries\n",
                  (unsigned)n, (int)doc.chapters.size(), (int)doc.toc.size());
    return true;
}
