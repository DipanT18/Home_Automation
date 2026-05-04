#pragma once

#include <cstdint>

// Runs the WiFiManager captive portal and returns true on successful connection.
bool runWiFiPortal(const char *apName, uint16_t timeoutSeconds, bool debugOutput);
