#ifndef SAVEGAME_H
#define SAVEGAME_H

#include "storage.h"
#include <M5Unified.h>
#include <SD.h>
#include <LittleFS.h>

// Write a text file to SD (or LittleFS fallback). Returns true on success.
bool saveGameWrite(const char* path, const String& content);

// Read a text file from SD (or LittleFS fallback). Returns empty on failure.
String saveGameRead(const char* path);

// Delete a save file from both SD and LittleFS.
void saveGameDelete(const char* path);

// Does a save file exist?
bool saveGameExists(const char* path);

#endif
