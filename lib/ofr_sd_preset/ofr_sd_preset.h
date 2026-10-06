#ifndef OFR_SD_PRESET_H
#define OFR_SD_PRESET_H

#include <SD.h>
#include <list>
#include <Arduino.h>

inline std::list<File>& ofr_file_list() {
    static std::list<File> list;
    return list;
}

inline FT_FILE *OFR_fopen(const char *filename, const char *mode) {
    const char* arduinoMode = FILE_READ;
    if (mode && (mode[0] == 'w' || mode[0] == 'a')) {
        arduinoMode = FILE_WRITE;
    }

    File f = SD.open(filename, arduinoMode);
    if (!f) {
        Serial.printf("[OFR] fopen FAILED: %s\n", filename);
        return nullptr;
    }

    f.seek(0);

    Serial.printf("[OFR] fopen: %s, size=%u, pos=%u\n",
                  filename, (unsigned)f.size(), (unsigned)f.position());

    ofr_file_list().push_back(f);
    return (FT_FILE*)&ofr_file_list().back();
}

inline void OFR_fclose(FT_FILE *stream) {
    if (!stream) return;

    auto& list = ofr_file_list();
    for (auto it = list.begin(); it != list.end(); ++it) {
        if ((FT_FILE*)&(*it) == stream) {
            it->close();
            list.erase(it);
            Serial.println("[OFR] fclose (entry removed)");
            return;
        }
    }

    // Fall back: assume the pointer is a File* we don't track.
    ((File *)stream)->close();
    Serial.println("[OFR] fclose (untracked)");
}

inline size_t OFR_fread(void *ptr, size_t size, size_t nmemb, FT_FILE *stream) {
    if (!stream || size == 0) return 0;
    File* f = (File*)stream;
    size_t bytesRead = f->read((uint8_t*)ptr, size * nmemb);
    return bytesRead / size;
}

inline int OFR_fseek(FT_FILE *stream, long int offset, int whence) {
    if (!stream) return -1;
    File* f = (File*)stream;

    uint32_t newPos = 0;
    switch (whence) {
        case SEEK_SET: newPos = (uint32_t)offset; break;
        case SEEK_CUR: newPos = f->position() + (uint32_t)offset; break;
        case SEEK_END: newPos = f->size() + (uint32_t)offset; break;
        default: return -1;
    }

    bool ok = f->seek(newPos);
    return ok ? 0 : -1;
}

inline long int OFR_ftell(FT_FILE *stream) {
    if (!stream) return -1;
    return ((File *)stream)->position();
}

#endif
