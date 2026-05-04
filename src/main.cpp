/**
 * @file    main.cpp
 * @project ESP32 Home Automation – Smart Temperature Monitor
 * @brief   Reads DHT11 temperature & humidity, serves a Wi-Fi web
 *          dashboard from LittleFS, exposes JSON API endpoints, and
 *          provides a plain-text /voice endpoint for Google Assistant
 *          / IFTTT Webhook integration.
 *
 * Endpoints
 * ---------
 *  GET /                 → Serves index.html from LittleFS
 *  GET /api/temperature  → JSON  { temperature_c, temperature_f, humidity }
 *  GET /api/status       → JSON  { ip, rssi, uptime_s, heap_free, … }
 *  GET /voice            → Plain text suitable for IFTTT/Google Assistant
 *
 * mDNS
 * ----
 *  http://homeauto.local  (on networks that support mDNS / Bonjour)
 *
 * Wi-Fi provisioning
 * ------------------
 *  First boot (or when no credentials are saved): ESP32 creates an AP
 *  called "HomeAuto-Setup". Connect with any phone/laptop and a captive
 *  portal lets you select your home network and enter the password.
 *  Credentials are saved in flash; subsequent reboots connect automatically.
 *
 * Hardware
 * --------
 *  DHT11 DATA → GPIO 4  (change DHT_PIN below if needed)
 *  DHT11 VCC  → 3.3 V
 *  DHT11 GND  → GND
 *  10 kΩ pull-up resistor between DATA and VCC (recommended)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "WiFiPortal.h"

// ─── Pin & sensor configuration ────────────────────────────────────────────
#define DHT_PIN     4          // GPIO connected to DHT11 DATA line
#define DHT_TYPE    DHT11

// ─── mDNS hostname  ────────────────────────────────────────────────────────
#define MDNS_HOSTNAME "homeauto"

// ─── Sensor polling interval (ms) ──────────────────────────────────────────
static constexpr unsigned long SENSOR_INTERVAL_MS = 5000UL;

// ─── Wi-Fi AP name used during captive-portal provisioning ─────────────────
static constexpr const char *WIFI_AP_NAME = "HomeAuto-Setup";

// ─── Global objects ─────────────────────────────────────────────────────────
DHT              dht(DHT_PIN, DHT_TYPE);
AsyncWebServer   server(80);

// ─── Cached sensor values ───────────────────────────────────────────────────
static float         g_tempC     = NAN;
static float         g_humidity  = NAN;
static unsigned long g_lastRead  = 0;
static unsigned long g_bootMs    = 0;

// ─── Forward declarations ───────────────────────────────────────────────────
static void    readSensor();
static String  buildTempJson();
static String  buildStatusJson();
static String  buildVoiceReply();
static void    setupRoutes();

// ============================================================================
//  setup()
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("\n[BOOT] ESP32 Home Automation — Smart Temperature Monitor"));

    g_bootMs = millis();

    // ── DHT11 ────────────────────────────────────────────────────────────────
    dht.begin();
    Serial.printf("[DHT11] Sensor initialised on GPIO %d\n", DHT_PIN);

    // ── LittleFS ─────────────────────────────────────────────────────────────
    if (!LittleFS.begin(true /* format on fail */)) {
        Serial.println(F("[FS]    LittleFS mount failed — dashboard will not be available."));
        Serial.println(F("[FS]    Run: pio run --target uploadfs"));
    } else {
        Serial.println(F("[FS]    LittleFS mounted OK"));
        // List files in root for debugging
        File root = LittleFS.open("/");
        File f    = root.openNextFile();
        while (f) {
            Serial.printf("[FS]      %-32s  %6u bytes\n", f.name(), (unsigned)f.size());
            f = root.openNextFile();
        }
    }

    // ── WiFiManager captive portal ───────────────────────────────────────────
    if (!runWiFiPortal(WIFI_AP_NAME, 180, false)) {
        Serial.println(F("[WiFi]  Config-portal timed out — restarting"));
        ESP.restart();
    }
    Serial.printf("[WiFi]  Connected  IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi]  SSID: %s  RSSI: %d dBm\n",
                  WiFi.SSID().c_str(), WiFi.RSSI());

    // ── mDNS ─────────────────────────────────────────────────────────────────
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[mDNS]  http://%s.local\n", MDNS_HOSTNAME);
    } else {
        Serial.println(F("[mDNS]  Failed to start — access via IP only"));
    }

    // ── Initial sensor read ───────────────────────────────────────────────────
    delay(2000);           // DHT11 stabilisation time
    readSensor();

    // ── HTTP routes & server start ────────────────────────────────────────────
    setupRoutes();
    server.begin();
    Serial.println(F("[HTTP]  Web server started on port 80"));
    Serial.println(F("[HTTP]  Ready — open browser at the IP shown above"));
}

// ============================================================================
//  loop()
// ============================================================================
void loop() {
    // ── Wi-Fi watchdog ────────────────────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[WiFi]  Connection lost — attempting reconnect…"));
        WiFi.reconnect();
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) {
            delay(500);
            Serial.print('.');
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi]  Reconnected  IP: %s\n",
                          WiFi.localIP().toString().c_str());
        } else {
            Serial.println(F("\n[WiFi]  Reconnect failed — will retry"));
        }
        return;
    }

    // ── Periodic sensor read ─────────────────────────────────────────────────
    if (millis() - g_lastRead >= SENSOR_INTERVAL_MS) {
        g_lastRead = millis();
        readSensor();
    }
}

// ============================================================================
//  readSensor()  — read DHT11 and update global cache
// ============================================================================
static void readSensor() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
        Serial.println(F("[DHT11] Read failed — keeping previous values"));
    } else {
        g_tempC    = t;
        g_humidity = h;
        Serial.printf("[DHT11] Temp: %.1f °C  Humidity: %.1f %%\n", t, h);
    }
}

// ============================================================================
//  JSON builders
// ============================================================================
static String buildTempJson() {
    JsonDocument doc;
    if (isnan(g_tempC)) {
        doc["error"] = "Sensor not ready or read failed";
    } else {
        // Round to 1 decimal place before storing in JSON
        float tC = roundf(g_tempC    * 10.0f) / 10.0f;
        float tF = roundf((g_tempC * 9.0f / 5.0f + 32.0f) * 10.0f) / 10.0f;
        float rh = roundf(g_humidity * 10.0f) / 10.0f;
        doc["temperature_c"] = tC;
        doc["temperature_f"] = tF;
        doc["humidity"]      = rh;
        doc["unit"]          = "Celsius";
    }
    String out;
    serializeJson(doc, out);
    return out;
}

static String buildStatusJson() {
    JsonDocument doc;
    doc["device"]    = "ESP32 Home Automation";
    doc["hostname"]  = String(MDNS_HOSTNAME) + ".local";
    doc["ip"]        = WiFi.localIP().toString();
    doc["ssid"]      = WiFi.SSID();
    doc["rssi"]      = WiFi.RSSI();
    doc["uptime_s"]  = (millis() - g_bootMs) / 1000UL;
    doc["heap_free"] = ESP.getFreeHeap();
    if (!isnan(g_tempC)) {
        doc["last_temp_c"]   = roundf(g_tempC    * 10.0f) / 10.0f;
        doc["last_humidity"] = roundf(g_humidity * 10.0f) / 10.0f;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

// ============================================================================
//  buildVoiceReply()  — plain text for Google Assistant / IFTTT
// ============================================================================
static String buildVoiceReply() {
    if (isnan(g_tempC)) {
        return F("Sorry, the temperature sensor is not responding right now.");
    }
    String reply  = "The current temperature is ";
    reply        += String(g_tempC, 1);
    reply        += " degrees Celsius";
    reply        += ", which is ";
    reply        += String(g_tempC * 9.0f / 5.0f + 32.0f, 1);
    reply        += " degrees Fahrenheit.";
    reply        += " The humidity is ";
    reply        += String(g_humidity, 1);
    reply        += " percent.";
    return reply;
}

// ============================================================================
//  setupRoutes()  — register all HTTP handlers on AsyncWebServer
// ============================================================================
static void setupRoutes() {
    // ── Dashboard (index.html) ─────────────────────────────────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (LittleFS.exists("/index.html")) {
            req->send(LittleFS, "/index.html", "text/html");
        } else {
            req->send(503, "text/html",
                "<h2>Dashboard not uploaded yet.</h2>"
                "<p>Run: <code>pio run --target uploadfs</code></p>");
        }
    });

    // ── Serve all other static files from LittleFS (CSS, JS, …) ──────────
    server.serveStatic("/", LittleFS, "/")
          .setCacheControl("max-age=600");

    // ── /api/temperature ──────────────────────────────────────────────────
    server.on("/api/temperature", HTTP_GET, [](AsyncWebServerRequest *req) {
        AsyncWebServerResponse *resp =
            req->beginResponse(200, "application/json", buildTempJson());
        resp->addHeader("Access-Control-Allow-Origin", "*");
        req->send(resp);
    });

    // ── /api/status ───────────────────────────────────────────────────────
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
        AsyncWebServerResponse *resp =
            req->beginResponse(200, "application/json", buildStatusJson());
        resp->addHeader("Access-Control-Allow-Origin", "*");
        req->send(resp);
    });

    // ── /voice — plain text for IFTTT / Google Assistant webhook ─────────
    server.on("/voice", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/plain", buildVoiceReply());
    });

    // ── 404 ───────────────────────────────────────────────────────────────
    server.onNotFound([](AsyncWebServerRequest *req) {
        req->send(404, "text/plain",
                  "404 – Not Found: " + req->url());
    });
}
