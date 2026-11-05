#include "time_sync.h"

#include <time.h>

static const int RTC_MIN_YEAR = 2024;

// ============================================================
// Portable UTC -> epoch seconds (no timegm dependency).
// ============================================================
static time_t utcToEpoch(const struct tm& t) {
    // Days since epoch, treating input as UTC.
    // Howard Hinnant's days_from_civil algorithm.
    int y = t.tm_year + 1900;
    int m = t.tm_mon + 1;   // 1..12
    int d = t.tm_mday;      // 1..31

    y -= (m <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = (long long)era * 146097LL + (long long)doe - 719468LL;

    long long secs = days * 86400LL
                   + t.tm_hour * 3600LL
                   + t.tm_min  * 60LL
                   + t.tm_sec;
    return (time_t)secs;
}

bool rtcLooksSet() {
    if (!M5.Rtc.isEnabled()) return false;
    auto dt = M5.Rtc.getDateTime();
    return dt.date.year >= RTC_MIN_YEAR;
}

String displayTimeFromRTC() {
    if (M5.Rtc.isEnabled()) {
        auto dt = M5.Rtc.getDateTime();

        struct tm tm_utc;
        memset(&tm_utc, 0, sizeof(tm_utc));
        tm_utc.tm_year = dt.date.year - 1900;
        tm_utc.tm_mon  = dt.date.month - 1;
        tm_utc.tm_mday = dt.date.date;
        tm_utc.tm_hour = dt.time.hours;
        tm_utc.tm_min  = dt.time.minutes;
        tm_utc.tm_sec  = dt.time.seconds;

        time_t utc_seconds = utcToEpoch(tm_utc);
        time_t local_seconds =
            utc_seconds + (TIME_SYNC_TZ_OFFSET_HOURS * 3600);

        struct tm tm_local;
        gmtime_r(&local_seconds, &tm_local);

        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d",
                 tm_local.tm_hour, tm_local.tm_min);
        return String(buf);
    }

    unsigned long totalSec = millis() / 1000;
    unsigned long hh = (totalSec / 3600) % 24;
    unsigned long mm = (totalSec / 60) % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", hh, mm);
    return String(buf);
}

static bool parseIsoUtc(const String& iso, struct tm& outTm) {
    memset(&outTm, 0, sizeof(outTm));
    const char* s = iso.c_str();
    const char* end = strptime(s, "%Y-%m-%dT%H:%M:%S", &outTm);
    if (end == nullptr) return false;
    if (*end != '\0' && !(*end == 'Z' && *(end + 1) == '\0')) return false;
    return true;
}

bool trySyncTimeFromSerial(unsigned long timeoutMs) {
    Serial.println("[Time] Waiting for host time sync (send: TIME=YYYY-MM-DDThh:mm:ssZ)");

    unsigned long start = millis();
    String buf;
    buf.reserve(64);

    while (millis() - start < timeoutMs) {
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                buf.trim();
                if (buf.startsWith("TIME=")) {
                    String iso = buf.substring(5);
                    struct tm t;
                    if (parseIsoUtc(iso, t)) {
                        if (M5.Rtc.isEnabled()) {
                            M5.Rtc.setDateTime(&t);
                            Serial.printf("[Time] RTC set from serial: %s\n", iso.c_str());
                            return true;
                        } else {
                            Serial.println("[Time] RTC not enabled; ignoring");
                            return false;
                        }
                    } else {
                        Serial.printf("[Time] Bad ISO timestamp: %s\n", iso.c_str());
                    }
                }
                buf = "";
            } else {
                buf += c;
            }
        }
        delay(10);
    }

    Serial.println("[Time] No serial sync received");
    return false;
}

bool ensureRtcTime() {
    if (rtcLooksSet()) {
        Serial.println("[Time] RTC already set — skipping sync");
        return true;
    }

    delay(500);
    if (trySyncTimeFromSerial(4000)) return true;

    Serial.println("[Time] RTC unset, no serial sync received");
    return false;
}
