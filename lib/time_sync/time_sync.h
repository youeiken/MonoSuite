#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <M5Unified.h>

// ---- RTC helpers -----------------------------------------------------------
// True if the RTC is enabled and looks plausibly set (year >= 2024).
bool rtcLooksSet();

// Format current time as "HH:MM" for the sleep clock.
// Adjusts UTC -> local via TZ_OFFSET_HOURS.
String displayTimeFromRTC();

// ---- NTP over WiFi ---------------------------------------------------------
// Requires WiFi to already be connected. Writes UTC into the RTC on success.
bool syncTimeFromNTP();

// ---- Host serial -----------------------------------------------------------
// Waits up to `timeoutMs` for a line "TIME=YYYY-MM-DDThh:mm:ssZ" on USB CDC.
// Writes UTC into the RTC on success. Returns true if the RTC was set.
bool trySyncTimeFromSerial(unsigned long timeoutMs = 3000);

// ---- Orchestrator ----------------------------------------------------------
// 1) If the RTC already looks set, returns true immediately (no radio).
// 2) Otherwise tries serial.
// 3) Otherwise (if `allowWifiFallback`) brings WiFi up, syncs, and powers
//    the radio back down. Returns true if the RTC ends up set.
bool ensureRtcTime(bool allowWifiFallback = true);

#endif
