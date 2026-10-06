#include "wifi_setup.h"

#include "display.h"
#include <LittleFS.h>
#include <SD.h>
#include <time.h>
#include <WiFi.h>

// ============================================================
// Load WiFi credentials from /wifi.txt
// Looks on SD first, then LittleFS.
// Returns true if both ssid and password were found.
// ============================================================
static bool loadWifiCredentials(String& ssid, String& password) {
    File f;

    if (isSdMounted() && SD.exists("/wifi.txt")) {
        f = SD.open("/wifi.txt", FILE_READ);
        Serial.println("[WiFi] Reading /wifi.txt from SD");
    } else if (LittleFS.exists("/wifi.txt")) {
        f = LittleFS.open("/wifi.txt", FILE_READ);
        Serial.println("[WiFi] Reading /wifi.txt from LittleFS");
    } else {
        Serial.println("[WiFi] /wifi.txt not found on SD or LittleFS");
        return false;
    }

    if (!f) return false;

    bool gotSsid = false;
    bool gotPass = false;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();

        // Skip empty lines and comments
        if (line.length() == 0) continue;
        if (line.startsWith("#")) continue;

        int eq = line.indexOf('=');
        if (eq < 0) continue;

        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        key.trim();
        val.trim();

        // Strip surrounding quotes if present: ssid="MyNet"
        if (val.length() >= 2 && val.charAt(0) == '"' && val.charAt(val.length() - 1) == '"') {
            val = val.substring(1, val.length() - 1);
        }

        if (key.equalsIgnoreCase("ssid")) {
            ssid = val;
            gotSsid = true;
        } else if (key.equalsIgnoreCase("password") || key.equalsIgnoreCase("pass")) {
            password = val;
            gotPass = true;
        }
    }

    f.close();

    if (!gotSsid) Serial.println("[WiFi] Missing 'ssid=' line");
    if (!gotPass) Serial.println("[WiFi] Missing 'password=' line");

    return gotSsid && gotPass;
}

bool isWifiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool startWifiSetup() {
    String ssid, password;

    if (!loadWifiCredentials(ssid, password)) {
        // Show error on screen
        beginDisplay();
        clearScreen();
        M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
        M5.Display.setFont(&fonts::FreeSansBold18pt7b);
        M5.Display.drawString("No WiFi config", 20, 200);
        M5.Display.setFont(&fonts::FreeSans12pt7b);
        M5.Display.drawString("Create /wifi.txt on SD:", 20, 280);
        M5.Display.drawString("ssid=YourNetwork", 20, 320);
        M5.Display.drawString("password=YourPass", 20, 360);
        M5.Display.drawString("Tap to continue", 20, 500);
        endDisplay();
        delay(3000);
        return false;
    }

    Serial.printf("[WiFi] Connecting to SSID: %s\n", ssid.c_str());

    beginDisplay();
    clearScreen();
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.drawString("Connecting...", 20, 200);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString(ssid, 20, 260);
    endDisplay();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Connected!");
        // syncTimeFromNTP();
        return true;
    }

    Serial.println("[WiFi] Connection failed");
    return false;
}

bool tryConnectSavedWifi() {
    // Same as before — this part doesn't use WiFiManager and should work.
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
        delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return false;
    }
    return true;
}

/* void syncTimeFromNTP() {
    if (!isWifiConnected()) {
        Serial.println("[NTP] Not connected, skipping sync");
        return;
    }

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC, two servers for redundancy

    Serial.print("[NTP] Syncing");
    time_t now = 0;
    int retries = 0;
    while (now < 100000 && retries < 30) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        retries++;
    }
    Serial.println();

    if (now < 100000) {
        Serial.println("[NTP] Sync failed");
        return;
    }

    // Store UTC time into the RTC (M5.Rtc expects UTC)
    if (M5.Rtc.isEnabled()) {
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        M5.Rtc.setDateTime(&timeinfo);
        Serial.printf("[NTP] RTC set to %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        Serial.println("[NTP] RTC not enabled — time only valid until reboot");
    }
} */

/* String displayTimeFromRTC() {
    if (M5.Rtc.isEnabled()) {
        auto dt = M5.Rtc.getDateTime();
        int hour = (dt.time.hours + 8) % 24;   // UTC+8, adjust as needed
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", hour, dt.time.minutes);
        return String(buf);
    } else {
        unsigned long totalSec = millis() / 1000;
        unsigned long hh = (totalSec / 3600) % 24;
        unsigned long mm = (totalSec / 60) % 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02lu:%02lu", hh, mm);
        return String(buf);
    }
} */
