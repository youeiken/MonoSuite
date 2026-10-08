#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <M5Unified.h>

// ---- RTC helpers -----------------------------------------------------------
// True if the RTC is enabled and looks plausibly set (year >= 2024).
bool rtcLooksSet();

// Format current time as "HH:MM" for the sleep clock.
// Adjusts UTC -> local via TZ_OFFSET_HOURS.
String displayTimeFromRTC();

// ---- Host serial -----------------------------------------------------------
// Waits up to `timeoutMs` for a line "TIME=YYYY-MM-DDThh:mm:ssZ" on USB CDC.
// Writes UTC into the RTC on success. Returns true if the RTC was set.
bool trySyncTimeFromSerial(unsigned long timeoutMs = 3000);

// ---- Orchestrator ----------------------------------------------------------
// 1) If the RTC already looks set, returns true immediately.
// 2) Otherwise waits for a serial time sync.
// Returns true if the RTC ends up set.
bool ensureRtcTime();

#endif
