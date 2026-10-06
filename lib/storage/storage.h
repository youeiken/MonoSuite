#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <LittleFS.h>

// ============================================================
// Storage module — owns the SD mount state and provides
// filesystem-agnostic helpers so consumers don't have to know
// whether a file lives on SD or LittleFS.
//
// The SD card is mounted by main.cpp's boot sequence (it needs
// hardware pin access). This module just records the outcome and
// exposes it, plus a couple of convenience wrappers.
// ============================================================

// ---- Mount state ---------------------------------------------------------

// True once the SD card has been mounted successfully at boot.
bool isSdMounted();

// Set the SD mount state. Only the boot sequence (or a future
// remount handler) should call this.
void setSdMounted(bool mounted);

// ---- Filesystem-agnostic file helpers ------------------------------------
//
// All three prefer SD when it's mounted and the file is present,
// then fall back to LittleFS. This is the same lookup order the
// project has used since day one, now in one place.

// Open a file for reading. Returns true on success.
bool storageOpenRead(const char* path, File& out);

// Open a file for writing. Returns true on success. Note: this
// does NOT guarantee the file is empty — SD.open(..., FILE_WRITE)
// appends if the file exists. Callers that need to truncate must
// delete first, as the existing games already do.
bool storageOpenWrite(const char* path, File& out);

// Does the file exist on SD or LittleFS?
bool storageExists(const char* path);

#endif
