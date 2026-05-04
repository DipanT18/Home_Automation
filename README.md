# 🏠 Home Automation — ESP32 Smart Temperature Monitor

A **PlatformIO-based ESP32 project** that reads temperature and humidity from a **DHT11 sensor**, exposes a real-time **Wi-Fi web dashboard** hosted directly on the ESP32 (via LittleFS), and provides a **voice-command endpoint** for Google Home / Google Assistant via IFTTT Webhooks.

---

## Table of Contents

1. [Features](#features)
2. [Architecture](#architecture)
3. [Hardware Requirements & Wiring](#hardware-requirements--wiring)
4. [Software Requirements](#software-requirements)
5. [Repository Structure](#repository-structure)
6. [Step-by-Step Setup](#step-by-step-setup)
   - [1 – Clone & Open in PlatformIO](#1--clone--open-in-platformio)
   - [2 – Install Library Dependencies](#2--install-library-dependencies)
   - [3 – Configure Wi-Fi (WiFiManager)](#3--configure-wi-fi-wifimanager)
   - [4 – Build & Upload Firmware](#4--build--upload-firmware)
   - [5 – Upload LittleFS (Dashboard UI)](#5--upload-littlefs-dashboard-ui)
   - [6 – Monitor Serial Output](#6--monitor-serial-output)
   - [7 – Open the Dashboard](#7--open-the-dashboard)
7. [API Reference](#api-reference)
8. [Voice Integration (Google Home via IFTTT)](#voice-integration-google-home-via-ifttt)
9. [Customisation](#customisation)
10. [Troubleshooting](#troubleshooting)
11. [License](#license)

---

## Features

| Feature | Details |
|---------|---------|
| **Sensor** | DHT11 – temperature (°C/°F) & relative humidity |
| **Wi-Fi provisioning** | WiFiManager captive portal – no hard-coded credentials |
| **Web dashboard** | Dark-mode, auto-refresh UI served from LittleFS |
| **REST API** | `/api/temperature` and `/api/status` (JSON) |
| **Voice endpoint** | `/voice` plain-text reply for IFTTT/Google Assistant |
| **mDNS** | Access at `http://homeauto.local` (no IP lookup needed) |
| **Async HTTP** | ESPAsyncWebServer – handles multiple clients simultaneously |
| **Wi-Fi watchdog** | Auto-reconnects if the connection drops |

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        ESP32 DevKit                         │
│                                                             │
│  GPIO 4 ──► DHT11 ──► readTemperature() / readHumidity()   │
│                              │                             │
│                         Global cache                        │
│                         g_tempC / g_humidity               │
│                              │                             │
│               ESPAsyncWebServer (port 80)                  │
│               ├── GET /            → LittleFS index.html   │
│               ├── GET /style.css   → LittleFS style.css    │
│               ├── GET /app.js      → LittleFS app.js       │
│               ├── GET /api/temperature → JSON              │
│               ├── GET /api/status      → JSON              │
│               └── GET /voice           → plain text        │
│                                                             │
│  LittleFS flash partition:                                  │
│  /index.html  /style.css  /app.js                          │
└─────────────────────────────────────────────────────────────┘
          │                              ▲
          │  Wi-Fi (2.4 GHz)             │ HTTP requests
          ▼                              │
┌──────────────────┐          ┌──────────────────────┐
│   Home Router    │          │  Browser / Google     │
│   (any Wi-Fi AP) │ ◄──────► │  Assistant / IFTTT    │
└──────────────────┘          └──────────────────────┘
```

**Data flow:**

1. DHT11 data pin → ESP32 GPIO 4 → firmware reads every 5 s and caches values.  
2. Browser loads `index.html` from LittleFS; JavaScript polls `/api/temperature` and `/api/status` every 10 s.  
3. IFTTT Webhook fires a `GET /voice` request; ESP32 returns a plain-English sentence read aloud by Google Assistant.

---

## Hardware Requirements & Wiring

### Components

| Component | Qty | Notes |
|-----------|-----|-------|
| ESP32 Dev Board (WiFi+BT) | 1 | e.g., DOIT ESP32 DevKit v1, NodeMCU-32S |
| DHT11 Temperature & Humidity Sensor | 1 | 3-pin or 4-pin module |
| 10 kΩ resistor | 1 | Pull-up on DATA line (often built into module) |
| Breadboard + jumper wires | – | |
| USB Micro-B / USB-C cable | 1 | For programming & power |

### Wiring Diagram

```
DHT11 Module          ESP32 DevKit
┌───────────┐         ┌────────────────┐
│  VCC  ────┼─────────┤ 3V3            │
│  DATA ────┼──[10kΩ]─┤ GPIO 4         │
│           │         │                │
│  GND  ────┼─────────┤ GND            │
└───────────┘         └────────────────┘
```

> **Note:** If your DHT11 module already has a pull-up resistor on the board, you do not need an external one.  
> The DATA pin is configurable — change `DHT_PIN` in `src/main.cpp` to any available GPIO.

---

## Software Requirements

| Tool | Version | Install |
|------|---------|---------|
| [Visual Studio Code](https://code.visualstudio.com/) | Latest | Download from website |
| [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) | Latest | VS Code Extension Marketplace |
| Python | 3.8 + | Required internally by PlatformIO |
| USB-to-Serial driver | – | CP210x or CH340 (depends on your ESP32 board) |

> **No Arduino IDE required.** PlatformIO manages all toolchains and libraries automatically.

---

## Repository Structure

```
Home_Automation/
├── platformio.ini          ← PlatformIO project configuration
├── src/
│   └── main.cpp            ← ESP32 firmware (all logic)
├── data/                   ← Files uploaded to LittleFS flash
│   ├── index.html          ← Dashboard web page
│   ├── style.css           ← Dashboard styles
│   └── app.js              ← Dashboard JavaScript (API polling)
└── README.md               ← This file
```

---

## Step-by-Step Setup

### 1 – Clone & Open in PlatformIO

```bash
git clone https://github.com/DipanT18/Home_Automation.git
cd Home_Automation
```

Open the folder in **VS Code** (`File → Open Folder…`). PlatformIO will detect `platformio.ini` automatically.

---

### 2 – Install Library Dependencies

PlatformIO installs all libraries listed in `platformio.ini` automatically on first build. You can also trigger it manually:

```bash
pio pkg install
```

Libraries installed:

| Library | Purpose |
|---------|---------|
| `adafruit/DHT sensor library` | DHT11/DHT22 driver |
| `adafruit/Adafruit Unified Sensor` | Abstraction layer (required by DHT) |
| `esphome/ESPAsyncWebServer-esphome` | Async HTTP server (espressif32 7.x compatible fork) |
| `esphome/AsyncTCP-esphome` | Async TCP layer (required by above) |
| `bblanchon/ArduinoJson` | JSON serialisation |
| `tzapu/WiFiManager` | Captive-portal Wi-Fi provisioning |

> **Note on library versions:** This project uses the **esphome-maintained forks** of ESPAsyncWebServer and AsyncTCP (`esphome/ESPAsyncWebServer-esphome ≥3.0` and `esphome/AsyncTCP-esphome ≥2.0`) instead of the original `me-no-dev` versions.  
> The original `me-no-dev/ESPAsyncWebServer` v1.x defines `HTTP_GET`, `HTTP_POST`, etc. as global enum values which conflict with the identically-named enum generated by the ESP-IDF `nghttp/http_parser.h` header that WiFiManager pulls in on **espressif32 7.x / Arduino-ESP32 3.x**.  The esphome forks ship an API-compatible rewrite that resolves this naming collision and build cleanly against the current platform.

---

### 3 – Configure Wi-Fi (WiFiManager)

This project uses **WiFiManager** — you do **not** need to hard-code your Wi-Fi credentials.

**First boot procedure:**

1. Power on the ESP32.
2. If no Wi-Fi credentials are saved, it creates a hotspot:  
   **SSID: `HomeAuto-Setup`** (no password)
3. Connect to `HomeAuto-Setup` from your phone or laptop.
4. A captive portal opens automatically (or navigate to `192.168.4.1`).
5. Tap **"Configure WiFi"**, select your home network, enter the password, and save.
6. The ESP32 restarts and connects to your home network.
7. Credentials are stored in flash — subsequent reboots connect automatically.

> **To reset credentials** (e.g., change network), hold the ESP32's BOOT/FLASH button during startup, or call `wm.resetSettings()` in code.

---

### 4 – Build & Upload Firmware

**Via VS Code PlatformIO toolbar:**
- Click the ✓ **Build** button (bottom toolbar)
- Click the → **Upload** button

**Via terminal:**

```bash
# Build only
pio run

# Build and upload
pio run --target upload
```

Expected serial output after successful boot:

```
[BOOT] ESP32 Home Automation — Smart Temperature Monitor
[DHT11] Sensor initialised on GPIO 4
[FS]    LittleFS mounted OK
[FS]      index.html                        3421 bytes
[FS]      style.css                         2876 bytes
[FS]      app.js                            1984 bytes
[WiFi]  Connected  IP: 192.168.1.42
[WiFi]  SSID: MyHomeNetwork  RSSI: -55 dBm
[mDNS]  http://homeauto.local
[DHT11] Temp: 26.0 °C  Humidity: 58.0 %
[HTTP]  Web server started on port 80
[HTTP]  Ready — open browser at the IP shown above
```

---

### 5 – Upload LittleFS (Dashboard UI)

The dashboard files in `data/` must be uploaded separately to the ESP32's flash filesystem.

> ⚠️ **Important:** Upload the firmware first (Step 4), then upload LittleFS.

**Via VS Code:**
1. Open the PlatformIO sidebar (ant icon).
2. Expand **`esp32dev`** → **Platform**.
3. Click **"Upload Filesystem Image"**.

**Via terminal:**

```bash
pio run --target uploadfs
```

You should see output similar to:

```
Building LittleFS image...
Uploading...
Writing at 0x00290000...
Wrote 49152 bytes
```

After a reset, open the dashboard in your browser.

---

### 6 – Monitor Serial Output

```bash
pio device monitor
```

Or in VS Code, click the **🔌 Serial Monitor** button in the bottom toolbar.

The monitor speed is set to **115200 baud** in `platformio.ini`.

Useful log lines to watch:

| Log prefix | Meaning |
|-----------|---------|
| `[DHT11]` | Sensor read results (every 5 s) |
| `[WiFi]`  | Connection status / reconnection events |
| `[HTTP]`  | Web server events |
| `[FS]`    | LittleFS mount & file listing |
| `[mDNS]`  | mDNS hostname registration |

---

### 7 – Open the Dashboard

Once the ESP32 is connected and LittleFS is uploaded:

```
http://homeauto.local       ← Works on most home networks (mDNS)
http://<ESP32-IP-address>   ← Always works (see IP in serial monitor)
```

The dashboard auto-refreshes every 10 seconds. You can also click **🔄 Refresh** manually.

---

## API Reference

All endpoints are served by the ESP32 on port 80.

### `GET /api/temperature`

Returns current sensor readings as JSON.

**Response (success):**
```json
{
  "temperature_c": 26.0,
  "temperature_f": 78.8,
  "humidity": 58.0,
  "unit": "Celsius"
}
```

**Response (sensor error):**
```json
{
  "error": "Sensor not ready or read failed"
}
```

---

### `GET /api/status`

Returns device diagnostics as JSON.

**Response:**
```json
{
  "device": "ESP32 Home Automation",
  "hostname": "homeauto.local",
  "ip": "192.168.1.42",
  "ssid": "MyHomeNetwork",
  "rssi": -55,
  "uptime_s": 3742,
  "heap_free": 214320,
  "last_temp_c": 26.0,
  "last_humidity": 58.0
}
```

---

### `GET /voice`

Returns a plain-text sentence suitable for Google Assistant / IFTTT.

**Response (success):**
```
The current temperature is 26.0 degrees Celsius, which is 78.8 degrees Fahrenheit. The humidity is 58.0 percent.
```

**Response (sensor error):**
```
Sorry, the temperature sensor is not responding right now.
```

---

## Voice Integration (Google Home via IFTTT)

This section explains how to trigger the ESP32's `/voice` endpoint using a **Google Assistant voice command** via **IFTTT Webhooks**.

### Prerequisites

- IFTTT account: [ifttt.com](https://ifttt.com)
- Your ESP32 must be accessible from the internet:
  - Option A: **Port forwarding** on your router (expose port 80 of the ESP32's local IP).
  - Option B: Use a **tunnel service** like [ngrok](https://ngrok.com) (great for testing).

> **Security note:** If you expose the ESP32 directly to the internet, consider adding HTTP Basic Auth or restricting access by IP. For a home-only setup, keep it local and use a different method.

---

### IFTTT Setup (Step-by-Step)

#### Step 1 – Create a new Applet

1. Log in to [ifttt.com](https://ifttt.com) and click **Create**.

#### Step 2 – Set the Trigger (IF This)

1. Click **Add** next to **If This**.
2. Search for and select **Google Assistant v2** (or "Google Assistant").
3. Choose **"Say a simple phrase"**.
4. Fill in:
   - **What do you want to say?** → `What is the temperature`
   - **What do you want the Assistant to say in response?** → `Checking the sensor, please wait.` *(temporary)*
5. Click **Save Trigger**.

#### Step 3 – Set the Action (THEN That)

1. Click **Add** next to **Then That**.
2. Search for and select **Webhooks**.
3. Choose **"Make a web request"**.
4. Fill in:
   - **URL:** `http://<YOUR-ESP32-IP-OR-PUBLIC-URL>/voice`
   - **Method:** `GET`
   - **Content Type:** `text/plain`
5. Click **Save Action**.

#### Step 4 – Connect Google Home

1. Open the **Google Home** app on your phone.
2. Go to **Settings → Works with Google**.
3. Search for **IFTTT** and link your IFTTT account.
4. Say to Google Assistant: **"Hey Google, What is the temperature"**

Google Assistant will trigger the IFTTT applet → IFTTT calls `GET /voice` on the ESP32 → ESP32 replies with the temperature sentence.

---

### Alternative: Direct Google Assistant Webhook (without IFTTT)

If you have a public URL for the ESP32, you can use [Make](https://www.make.com) or [Zapier](https://zapier.com) as webhook intermediaries with more flexible response handling.

For advanced users: Google Cloud Functions or Actions on Google can forward the spoken response text back to the user in real time.

---

## Customisation

| Setting | File | How to change |
|---------|------|--------------|
| DHT11 GPIO pin | `src/main.cpp` | Change `#define DHT_PIN 4` |
| Sensor type (DHT22) | `src/main.cpp` | Change `#define DHT_TYPE DHT11` → `DHT22` |
| mDNS hostname | `src/main.cpp` | Change `#define MDNS_HOSTNAME "homeauto"` |
| Sensor poll interval | `src/main.cpp` | Change `SENSOR_INTERVAL_MS` |
| Dashboard refresh rate | `data/app.js` | Change `const REFRESH_INTERVAL = 10` |
| WiFi AP name | `src/main.cpp` | Change `WIFI_AP_NAME` |
| Board type | `platformio.ini` | Change `board = esp32dev` |

---

## Troubleshooting

### Dashboard shows `--` after loading

- LittleFS files may not be uploaded. Run: `pio run --target uploadfs`
- Ensure the ESP32 is connected to Wi-Fi (check serial monitor).

### `DHT11 Read failed` in serial monitor

- Check wiring: VCC → 3.3V, GND → GND, DATA → GPIO 4.
- Add a 10 kΩ pull-up resistor between DATA and VCC if not on module.
- DHT11 needs up to 2 seconds to stabilise after power-on — the firmware waits automatically.
- Try a shorter wire between ESP32 and DHT11.

### Cannot open `http://homeauto.local`

- mDNS (`*.local`) requires Bonjour/mDNS support on the client device.
  - Windows: Install [Bonjour Print Services](https://support.apple.com/kb/DL999) or use the raw IP.
  - Linux: `sudo apt install avahi-daemon`
  - macOS/iOS/Android: Works natively.
- Use the raw IP address instead: shown in the serial monitor.

### WiFiManager captive portal does not appear

- Connect to `HomeAuto-Setup` hotspot.
- Navigate manually to `http://192.168.4.1` in a browser.
- Disable mobile data on your phone so it doesn't bypass the portal.

### Build error: `'HTTP_GET' conflicts with a previous declaration`

This error occurs when the original **me-no-dev/ESPAsyncWebServer** library is used together with **WiFiManager** and **espressif32 7.x / Arduino-ESP32 3.x**.

**Root cause:** `me-no-dev/ESPAsyncWebServer` v1.x defines `HTTP_GET`, `HTTP_POST`, `HTTP_DELETE`, etc. as global enum values.  WiFiManager includes Arduino's `WebServer.h` which in turn includes the ESP-IDF `nghttp/http_parser.h`.  That header defines the same names via a macro expansion — the C++ compiler sees two declarations for the same identifier and refuses to compile.

**Fix (already applied):** `platformio.ini` uses the esphome-maintained fork `esphome/ESPAsyncWebServer-esphome ≥3.0` with `esphome/AsyncTCP-esphome ≥2.0`.  These forks resolve the naming collision and are API-compatible with the me-no-dev originals.

If you forked this repo and still see the error, ensure your `lib_deps` does **not** contain `me-no-dev/ESPAsyncWebServer` or `me-no-dev/AsyncTCP`.  Delete the `.pio/libdeps` cache folder and rebuild:

```bash
rm -rf .pio/libdeps
pio run --target upload
```

---

### Upload fails with "No serial port found"

- Install the correct USB-to-serial driver for your board (CP210x or CH340).
- On Linux, add your user to the `dialout` group: `sudo usermod -aG dialout $USER` then log out/in.

### LittleFS upload fails

- Make sure the firmware is already uploaded and the ESP32 is not in bootloader mode.
- Try pressing the **RESET** button on the ESP32 after firmware upload, then run `uploadfs`.

### ESP32 keeps restarting (boot loop)

- Open serial monitor — look for the panic/exception message.
- The most common cause is a Wi-Fi credential issue. Connect to `HomeAuto-Setup` and re-enter credentials.

---

## License

This project is released under the [MIT License](https://opensource.org/licenses/MIT).

---

*Built with ❤️ using PlatformIO, ESP32-Arduino, ESPAsyncWebServer (esphome fork), DHT sensor library, WiFiManager, and ArduinoJson.*
