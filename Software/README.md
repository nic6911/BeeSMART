# BeeSMART honeyDosing — Build & Upload Instructions

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| [Arduino CLI](https://arduino.github.io/arduino-cli/) | ≥ 1.2.x | Or Arduino IDE 2.x |
| ESP32 board package | `esp32:esp32` ≥ 3.3.x | Install via board manager |
| [mklittlefs](https://github.com/earlephilhower/mklittlefs) | Bundled with ESP32 package | Used to build the filesystem image |
| [esptool](https://github.com/espressif/esptool) | Bundled with ESP32 package | Used to flash the filesystem image |

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

## Uploading the Firmware

```bash
arduino-cli upload --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc -p COM15 honeyDosing_v3/
```

Replace `COM15` with your actual COM port.

## Uploading the Filesystem (Web UI)

The `honeyDosing_v3/data/` folder contains the web interface files (HTML, CSS, JS). These
must be uploaded as a LittleFS filesystem image to the ESP32.

### Step 1 — Build the LittleFS image

```bash
mklittlefs -c honeyDosing_v3/data -p 256 -b 4096 -s 1441792 littlefs.bin
```

The paths to `mklittlefs` and `esptool` are typically found inside the Arduino data
folder:

- **Windows:** `%LOCALAPPDATA%\Arduino15\packages\esp32\tools\mklittlefs\<version>\mklittlefs.exe`
- **macOS/Linux:** `~/.arduino15/packages/esp32/tools/mklittlefs/<version>/mklittlefs`

### Step 2 — Flash the image

```bash
esptool --chip esp32c3 --port COM15 --baud 460800 write_flash 0x290000 littlefs.bin
```

The offset `0x290000` corresponds to the default 4 MB partition table with a spiffs/littlefs
partition. Adjust if using a custom partition scheme.

> **Tip:** After uploading, connect to the **BeeSMART** Wi-Fi network and navigate to
> `beesmart.local` (or `192.168.4.1`) in a browser to access the interface.

## Project Structure

```
Software/
├── honeyDosing_v3/
│   ├── honeyDosing_v3.ino   # Main firmware source
│   └── data/                 # Web UI assets (uploaded as LittleFS)
│       ├── index.html
│       ├── app.js
│       └── styles.css
├── CHANGELOG.md              # Version history
└── README.md                 # This file
```

## Troubleshooting

- **Upload fails / no COM port:** Ensure the ESP32-C3 is connected via its native USB
  port. Hold the BOOT button while pressing RESET to enter download mode if needed.
- **Web UI not loading after firmware upload:** Make sure to also upload the LittleFS
  filesystem image (Step 2 above).
- **ArduPID compile errors:** Verify you have version **0.2.1**, not 1.0.x. The API
  changed significantly between versions.
