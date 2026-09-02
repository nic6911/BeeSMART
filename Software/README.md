# BeeSMART honeyDosing — Build & Upload Instructions (v3.3.0)

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| [Arduino CLI](https://arduino.github.io/arduino-cli/) | ≥ 1.2.x | Or Arduino IDE 2.x |
| ESP32 board package | `esp32:esp32` ≥ 3.3.x | Install via board manager |
| [mklittlefs](https://github.com/earlephilhower/mklittlefs) | Bundled with ESP32 package | Used to build the filesystem image |

### Arduino Libraries

Install via the Arduino Library Manager or `arduino-cli lib install`:

| Library | Version |
|---------|---------|
| ArduinoJson | ≥ 7.4.x |
| ESP32Servo | ≥ 3.1.x |
| HX711 | ≥ 0.6.x |
| ArduPID | **0.2.1** (do NOT use 1.0.x — incompatible API) |

> **ESPmDNS** is part of the ESP32 board package and does not need a separate install.

## Hardware

- **Board:** ESP32-C3 (e.g. ESP32-C3-WROOM-02)
- **Connection:** USB-Serial/JTAG (native USB on the C3)
- **COM port:** Check Device Manager — the device appears as "USB JTAG/serial debug unit"

## Building the Firmware

### Using Arduino CLI

```bash
# Install ESP32 board support (first time only)
arduino-cli core install esp32:esp32

# Install required libraries (first time only)
arduino-cli lib install ArduinoJson@7.4.3 ESP32Servo@3.1.3 HX711@0.6.3 ArduPID@0.2.1

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc honeyDosing_v3/
```

### Using Arduino IDE

1. Open `honeyDosing_v3/honeyDosing_v3.ino`
2. Select board: **ESP32C3 Dev Module**
3. Set **USB CDC On Boot: Enabled**
4. Click **Verify/Compile**

## Building the Filesystem Image

Run `build_merge.bat` to create the LittleFS image containing the web UI:

```
.\build_merge.bat
```

This creates `build/littlefs.bin` (1.38 MB).

## Uploading

### Firmware

```bash
# Via arduino-cli
arduino-cli upload --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc -p COM15 honeyDosing_v3/
```

Replace `COM15` (or `/dev/ttyACM0` on Linux) with your actual port.

### Filesystem (Web UI)

Flash the LittleFS image at offset `0x290000`:

```bash
# Via esptool
esptool.py --chip esp32c3 -p COM15 write-flash 0x290000 build/littlefs.bin
```

> **Tip:** After uploading both, connect to the **BeeSMART** Wi-Fi network and navigate to `beesmart.local` (or `192.168.4.1`) in a browser.

## Project Structure

```
honeyDosing_v3/
├── honeyDosing_v3.ino    # Entry point (setup + loop only)
├── src/                   # Modular firmware source
│   ├── config.h           # Constants, pins, structs
│   ├── globals.h/.cpp     # Global variables + language strings
│   ├── filesystem.h/.cpp  # File I/O, settings persistence
│   ├── statistics.h/.cpp  # Dispensing records, weight sampling
│   ├── control.h/.cpp     # PID, state machines (dosing/calibration)
│   └── webserver.h/.cpp   # HTTP API, WebSocket, captive portal
├── data/                  # Web UI assets (uploaded as LittleFS)
│   ├── index.html
│   ├── app.js
│   ├── styles.css
│   └── beesmart_bee.webp  # 62KB logo (was 521KB PNG)
├── build_merge.bat        # LittleFS image builder (Windows)
├── CHANGELOG.md           # Version history
└── README.md              # This file
```

## Troubleshooting

- **Upload fails / no COM port:** Ensure the ESP32-C3 is connected via its native USB port. Hold the BOOT button while pressing RESET to enter download mode if needed.
- **Web UI not loading after firmware upload:** Make sure you also flashed the LittleFS filesystem image at `0x290000`.
- **ArduPID compile errors:** Verify you have version **0.2.1**, not 1.0.x. The API changed significantly between versions.
