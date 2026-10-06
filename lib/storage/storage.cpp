#include "storage.h"

// ============================================================
// Mount state
// ============================================================
static bool s_sdMounted = false;

bool isSdMounted()                  { return s_sdMounted; }
void setSdMounted(bool mounted)     { s_sdMounted = mounted; }

// ============================================================
// Filesystem-agnostic file helpers
// ============================================================

bool storageExists(const char* path) {
    if (path == nullptr) return false;
    if (s_sdMounted && SD.exists(path)) return true;
    if (LittleFS.exists(path)) return true;
    return false;
}

bool storageOpenRead(const char* path, File& out) {
    if (path == nullptr) return false;

    if (s_sdMounted && SD.exists(path)) {
        File tmp = SD.open(path, FILE_READ);
        if (tmp) { out = tmp; return true; }
    }

    if (LittleFS.exists(path)) {
        File tmp = LittleFS.open(path, FILE_READ);
        if (tmp) { out = tmp; return true; }
    }

    return false;
}

bool storageOpenWrite(const char* path, File& out) {
    if (path == nullptr) return false;

    if (s_sdMounted) {
        File tmp = SD.open(path, FILE_WRITE);
        if (tmp) { out = tmp; return true; }
        // Fall through: if SD.open fails, try LittleFS as a last
        // resort. This shouldn't normally happen, but it's better
        // than dropping the write.
    }

    File tmp = LittleFS.open(path, FILE_WRITE);
    if (tmp) { out = tmp; return true; }

    return false;
}
