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

On 2026-10-09, replacing the USB cable changed the CP210x BUSID to `1-8` (serial `01AB9FF2`). The isolated board then booted without brownout, and the application-only flashes completed with esptool hash verification. Serial showed `BOOT done` and `BasicMode: Setup complete`, while the read-only smoke check passed after a reset. This supports a power/cable cause for the earlier brownouts, but does not prove long-term power margin. Check `MAINTENANCE.md` for the latest flash evidence. The changed `data/setup.html` is not on the device until a deliberate SPIFFS update; do not use `uploadfs` with an incomplete private filesystem image.

## Product direction

iYak32 is a small family of ESP32 projects, starting with repeatable room sensor nodes for the garage, basement, and attic. The next uses are the iGrow garden and a small proximity/camera robot. The kayak controller and nearly finished hockey prototype remain real projects, but no longer dictate the first features. Keep one shared MQTT/sensor foundation and small, explicit device profiles; do not make every board load every feature.

The first isolated board already publishes six numeric measurements on `sensors/ESP_15605/<measure>` to the LAN broker. Its BMP280 does not measure humidity. Home Assistant can consume these topics through its MQTT integration and discovery, once connected to the same private broker. Each physical board needs a stable unique identity and a friendly room name; `ESP_15605` is the current board's existing identity.

## Top 25 features worth building

These are product slices, not claims that the current hardware supports them. Prioritize the home nodes now, garden and robot next, then finish the kayak and hockey experiences with their own hardware.

| # | Project | Feature | First useful slice |
|---:|---|---|---|
| 1 | Home | Per-board identity and room label | Name a board Garage, Basement, or Attic while keeping a stable ID in MQTT topics. |
| 2 | Home | Home Assistant discovery | Publish retained MQTT sensor descriptions so a board appears as one HA device. |
| 3 | Home | Room climate card | Show temperature, pressure, Wi-Fi, and power status for one installed node. |
| 4 | Home | Whole-house comparison | Put garage, basement, attic, and outdoor temperature on one graph. |
| 5 | Home | Humidity and dew point | Add a BME280 or other humidity sensor and flag a damp basement. |
| 6 | Home | Freeze and heat alerts | Set room-specific thresholds in Home Assistant for garage cold and attic heat. |
| 7 | Home | Motion timeline | Publish PIR motion as an MQTT binary sensor and show recent events. |
| 8 | Home | Device online/offline | Expose MQTT availability so a missing node is visible instead of showing stale data. |
| 9 | Home | Placement check | Compare a new node with a reference thermometer before mounting it. |
| 10 | Home | Quick install card | A simple label with device ID, room, IP, sensor type, and local UI address. |
| 11 | Garden | Soil moisture zones | Read one calibrated probe per bed or planter and graph dry/wet trends. |
| 12 | Garden | Watering dashboard | Show soil moisture, last watering, and next planned watering together. |
| 13 | Garden | Manual watering timer | Start a pump/valve for a bounded duration and show when it stops. |
| 14 | Garden | Rain-aware watering | Skip a planned cycle when soil is wet or rain is expected. |
| 15 | Garden | Greenhouse climate | Track temperature/humidity and alert on frost or overheating. |
| 16 | Garden | Plant journal | Attach a watering or harvest note to the local sensor history. |
| 17 | Robot | Proximity radar | Show front/side distance sensors live while the robot is stationary. |
| 18 | Robot | Drive pad | Phone controls with a dead-man release and visible motor state. |
| 19 | Robot | Camera view | Add a local stream or snapshots from suitable camera hardware. |
| 20 | Robot | Patrol moments | Save a timestamped snapshot when proximity or motion fires. |
| 21 | Kayak | Live cockpit | One phone view of weather, battery, speed input, and mode. |
| 22 | Kayak | Outing markers | One tap records a catch/spot with time and weather; GPS can come later. |
| 23 | Kayak | Motor input preview | Calibrate throttle and steering with motor output disconnected before drive work. |
| 24 | Hockey | Finish live scoreboard | Complete physical goal detection, timer, score display, and phone view. |
| 25 | Hockey | Goal celebration and recap | Add side-specific LED/sound themes and a shareable match summary. |

Build the first installed room node with #1–3 and #8, then use the second board to prove identities, room labels, and comparison. Garden actuators and robot drive controls need their own physical tests. Maintenance tasks and flash evidence live in `MAINTENANCE.md`. Web-page changes require a deliberate SPIFFS update that preserves private files.
