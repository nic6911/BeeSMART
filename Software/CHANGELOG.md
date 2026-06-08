# Changelog

## v3.3.0

### Improvements

- **Modular project structure** — Monolithic 1462-line `.ino` file split into `src/` modules:
  `globals`, `filesystem`, `statistics`, `control`, `webserver`. The `.ino` entry point
  is now 134 lines (setup + loop only).
- **WebSocket broadcast tiers** — Lightweight weight-only JSON (25 Hz) replaces full
  status broadcasts every loop cycle. Full status (including statistics) broadcasts
  once per second, reducing browser parsing load.
- **Immediate auto-off on stop** — Pressing the Stop button now immediately unchecks the
  "Auto Start" toggle locally, eliminating the 10-second polling delay.
- **WebP logo** — `beesmart_bee.png` (521 KB) compressed to `beesmart_bee.webp` (62 KB)
  using quality 80, reducing the LittleFS data directory from 640 KB to 188 KB.
- **Immediate viscosity PID update** — Changing the viscosity preset now calls
  `fetchSettings()` immediately, eliminating the 10-second delay for PID field updates
  in the UI.
- **LittleFS image builder** — `build_merge.bat` builds the filesystem image from the
  `data/` directory. Flash it at offset `0x290000`.

### Bug Fixes

- **Stop button does not toggle auto start off** — The client-side stop handler now
  immediately sets `autoState = false` and updates the toggle, and the WebSocket
  handler syncs `autoState` from incoming data for consistency.
- **Input validation regressions** — Restored `setAmount` clamping to `[minWeight, maxWeight]`,
  `setLanguage` range constraint to 0–2, and `start` command now checks `calStateMachine == 0`.
- **Unknown API commands return 400** — Unknown commands no longer silently return success.
- **Slider snap-back race condition** — Debounced slider commands are tracked; settings
  updates from the server skip fields with pending local changes. Slider values are not
  overwritten while the user is dragging (focus-aware).

## v3.2.0

### Bug Fixes

- **Number input fields no longer revert while typing** — The settings polling loop
  previously overwrote input fields every 2 seconds, causing user input to be lost.
  Fields that have focus are now skipped during UI updates.
- **Slider snap-back fixed** — Servo position sliders no longer jump back to stale
  values while the user is dragging them. Settings with pending commands are skipped
  during polling updates.
- **Ti=0 division by zero in PID** — The high-viscosity preset sets Ti=0; the integral
  gain calculation now guards against division by zero.
- **CSV parsing bug for PID presets 1–3** — A substring index error caused kP values for
  presets 1–3 to include a leading comma, parsing to 0. Fixed the start-index logic.
- **Division by zero when idle** — `adjustedWeight / setpoint` in the main loop now
  guards against setpoint=0.
- **API input validation** — `gainSelector` is clamped to 0–3, `lang` to 0–2,
  `calWeight` minimum enforced at 1, `maxWeight` clamped to valid range.
  `deserializeJson` return value is now checked with a 400 error response on failure.
- **snprintf buffer overflow** — WiFi SSID buffer increased to 26 bytes with `sizeof()`
  used for safety.
- **Hardcoded Danish string** — Calibration step 2 text now uses the translation array
  `calStep2Text[lang]` instead of a hardcoded Danish string.
- **Settings polling interval leak** — The `setInterval` ID is now properly stored and
  cleared before creating a new interval, preventing memory leaks when switching tabs.
- **setpointPI normalisation** — Fixed `setpointPI = setpoint / setpoint` (always 1.0)
  to use the correct normalised value.

### New Features

- **mDNS hostname** — The device now advertises itself as `beesmart.local` via mDNS.
  Users can type `beesmart.local` in a browser instead of `192.168.4.1`.
- **Captive portal improvement** — Unknown host requests are served the UI page directly
  instead of issuing a redirect, improving compatibility with mobile captive portal
  browsers.
- **Glass detection filter** — A 2-second millis()-based filter ensures the glass is
  stably on the scale before the filling process starts, preventing false triggers.
- **Max tapping amount increased** — The maximum allowable weight is now 25,000 g (was
  20,000 g). Default remains 1,000 g.
- **Servo button highlighting** — The "Go to Min" / "Go to Max" servo buttons now
  maintain a highlighted state showing which position was last selected. "Go to Min"
  is highlighted by default on page load.

### Improvements

- **Slider HTTP debounce** — Slider value changes are debounced (250 ms) before sending
  HTTP requests, reducing network load by ~95% during slider drags.
- **server.handleClient() timing** — HTTP request handling moved outside the 20 ms timer
  gate for more responsive web UI.
- **Blocking delay removed** — The 100 ms `delay()` call after file writes has been
  removed; `file.close()` is already synchronous.
- **Cache-Control headers** — CSS and JS responses include `Cache-Control: no-cache` to
  prevent stale cached assets after firmware updates.

## v3.1.0

- Initial public release of honeyDosing v3.
