#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include "storage.h"
#include <M5Unified.h>

bool isWifiConnected();
bool startWifiSetup();
bool tryConnectSavedWifi();

// void syncTimeFromNTP();
// String displayTimeFromRTC();

#endif
