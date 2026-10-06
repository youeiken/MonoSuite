#include "savegame.h"

bool saveGameWrite(const char* path, const String& content) {
    File f;
    if (!storageOpenWrite(path, f)) {
        Serial.printf("[Save] Failed to open %s for write\n", path);
        return false;
    }
    size_t written = f.print(content);
    f.close();
    Serial.printf("[Save] Wrote %d bytes to %s\n", (int)written, path);
    return written == content.length();
}

String saveGameRead(const char* path) {
    File f;
    if (!storageOpenRead(path, f)) return "";
    String s = f.readString();
    f.close();
    return s;
}

void saveGameDelete(const char* path) {
    if (isSdMounted() && SD.exists(path)) SD.remove(path);
    if (LittleFS.exists(path)) LittleFS.remove(path);
}

bool saveGameExists(const char* path) {
    return storageExists(path);
}
