# iYak32 relaunch guide

## What runs where

This is one ESP32 firmware built with PlatformIO and Arduino. `src/main.cpp` selects the mode at compile time: `BASIC_MODE` is currently enabled; `BOAT` and `HOCKEY_MODE` are disabled. The state machine starts shared services, configures the selected mode, then polls it and publishes periodic telemetry.

`src/api/Esp32.cpp` mounts SPIFFS, reads `/io_config.json` and `/esp32config.json`, applies GPIO settings, and starts Wi-Fi, time sync, buzzer, and MQTT. `src/api/WifiManager.cpp` handles station/AP fallback and OTA. `src/api/Mqtt.cpp` handles reconnect and publishing; `MqttCommandRouter.cpp` receives commands. `src/www.h` defines the HTTP API and serves the files in `data/` from SPIFFS. `src/api/devices/` contains sensors, display, and buzzer code. `BasicMode.h`, `Boat.h`, and `Hockey.h` are the mode implementations.

The files `data/esp32config.json`, `data/config.txt`, and `data/mqtt.txt` are private local inputs and are ignored by Git. The tracked `*.example.*` files are templates. A firmware flash and a filesystem upload are different operations: `uploadfs` can replace the device's private SPIFFS contents.

## Build and device workflow on devX

1. Use the private SSH alias `devX` and the checkout at `~/codes/iYak32`. Build with `~/.venvs/iyak32/bin/pio run -e esp32dev`. There is no unit test suite in this repository yet.
2. Confirm the CP210x USB bridge identity before attaching it from Windows to devX with USB/IP. Keep port 3240 on the private LAN and restrict access to devX. The Linux port is `/dev/ttyUSB0` when attached.
3. Read the partition table and active OTA slot before flashing. This board has a 4 MB flash and separate application and SPIFFS partitions. Preserve a full private flash backup outside Git before the first write.
4. Flash only the active application partition when changing firmware; do not upload the filesystem or rewrite the partition table for an ordinary code change. Verify the image hash, check the serial boot log, then run `python3 scripts/smoke_device.py <device-ip>` from devX.
5. If the serial log reports a brownout, stop further flashes and check USB power/cable stability. A successful esptool write proves flash transport, not a successful application boot.

On 2026-10-09, replacing the USB cable changed the CP210x BUSID to `1-8` (serial `01AB9FF2`). The isolated board then booted without brownout, and the application-only flashes completed with esptool hash verification. The current image SHA-256 is `a52d1daa1699bc45fb84d2ec442166b6e488c0e9fdd0e62f8714ee8c47aac5fe`; serial shows `BOOT done` and `BasicMode: Setup complete`, while the read-only smoke check passed after a reset. This supports a power/cable cause for the earlier brownouts, but does not prove long-term power margin. The changed `data/setup.html` is not on the device until a deliberate SPIFFS update; do not use `uploadfs` with an incomplete private filesystem image.

## Top 25 features worth building

A mix of kayak, ESP32 bench, and hockey experiences. `BASIC_MODE` is the active firmware; boat and hockey need their own hardware before device acceptance. These are product ideas, not claims that the current device already supports them.

| # | Mode | Feature | First useful slice |
|---:|---|---|---|
| 1 | Shared | Live mission cockpit | One phone-sized view of weather, battery, connection, activity, and active mode. |
| 2 | Shared | Fishing-weather trend | Rising/falling/steady pressure with a short BMP280 chart. |
| 3 | Shared | Event timeline | Show button presses, readings, reconnects, and mode events in time order. |
| 4 | Shared | Local MQTT sensor feed | Publish measurements as `sensors/<device>/<measure>` on the LAN broker. |
| 5 | Bench | Demo mode | Animate believable readings and events while actuators are unplugged. |
| 6 | Bench | Analog calibration wizard | Plot noise and guide min/max calibration for the two input channels. |
| 7 | Bench | Button gestures | Give click, double click, and long press useful assignable actions. |
| 8 | Bench | LED and sound scenes | Pick a few playful visual/audio presets from the UI. |
| 9 | Bench | Three-button reaction game | Score a short game using real buttons, LEDs, and the phone screen. |
| 10 | Boat | Outing timer | Start, pause, and end a fishing trip, even before GPS is available. |
| 11 | Boat | Catch and spot markers | One tap records time, temperature, pressure, and a short note. |
| 12 | Boat | Trip recap | Replay events and export a local JSON/CSV trip card. |
| 13 | Boat | Input preview | Show throttle, direction switch, and steering input with motor output disabled. |
| 14 | Boat | Drive profiles | Eco, Troll, and Transit presets with speed caps and smooth ramps. |
| 15 | Boat | Battery gauge | Calibrated voltage trend, low-battery cue, and current when available. |
| 16 | Boat | OLED quick pages | Cycle through weather, battery, trip time, and motor state. |
| 17 | Boat | Tide strip | Next high/low tide and a cached short forecast in the cockpit. |
| 18 | Boat | Route and return cue | Local GPS breadcrumbs plus bearing/distance back to launch when GPS exists. |
| 19 | Hockey | Spectator scoreboard | Live score, period, timer, and game state on nearby phones. |
| 20 | Hockey | Goal celebration themes | Different LED, buzzer, and screen effects for each side. |
| 21 | Hockey | Team and rule presets | Names, period lengths, and sound choices set before a match. |
| 22 | Hockey | Practice mode | Trigger, calibrate, and visualize both goal sensors before play. |
| 23 | Hockey | Goal timeline | Replay scoring, pauses, and period changes after the game. |
| 24 | Hockey | Tournament board | Track a small round robin with wins and goal differences. |
| 25 | Hockey | Shareable match card | Export the final score and key moments from the browser. |

First wave: #1–5 on the isolated board, then pick one tactile bench feature and one hockey feature for the relevant hardware. Boat motor controls follow input preview, calibration, and a neutral/failsafe check. A virtual anchor remains a later experiment once GPS and motor control are proven. Maintenance tasks and flash evidence live in `MAINTENANCE.md`. Web-page changes require a deliberate SPIFFS update that preserves private files.
