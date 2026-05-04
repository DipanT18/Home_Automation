#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "WiFiPortal.h"

// Kept in a separate translation unit to avoid HTTP_* enum collisions with ESPAsyncWebServer.
bool runWiFiPortal(const char *apName, uint16_t timeoutSeconds, bool debugOutput) {
    WiFiManager wm;
    wm.setConfigPortalTimeout(timeoutSeconds);
    wm.setDebugOutput(debugOutput);
    const char *portalName = (apName && *apName) ? apName : "AutoConnectAP";
    return wm.autoConnect(portalName);
}
