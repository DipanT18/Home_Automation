#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "WifiPortal.h"

bool runWiFiPortal(const char *apName, uint16_t timeoutSeconds, bool debugOutput) {
    WiFiManager wm;
    wm.setConfigPortalTimeout(timeoutSeconds);
    wm.setDebugOutput(debugOutput);
    return wm.autoConnect(apName);
}
