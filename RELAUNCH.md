# iYak32 relaunch guide

## What runs where

This is one ESP32 firmware built with PlatformIO and Arduino. `src/main.cpp` selects the mode at compile time: `BASIC_MODE` is currently enabled; `BOAT` and `HOCKEY_MODE` are disabled. The state machine starts shared services, configures the selected mode, then polls it and publishes periodic telemetry.

`src/api/Esp32.cpp` mounts SPIFFS, reads `/io_config.json` and `/esp32config.json`, applies GPIO settings, and starts Wi-Fi, time sync, buzzer, and MQTT. `src/api/WifiManager.cpp` handles station/AP fallback and OTA. `src/api/Mqtt.cpp` handles reconnect and publishing; `MqttCommandRouter.cpp` receives commands. `src/www.h` defines the HTTP API and serves the files in `data/` from SPIFFS. `src/api/devices/` contains sensors, display, and buzzer code. `BasicMode.h`, `Boat.h`, and `Hockey.h` are the mode implementations.

The gitignored `data/esp32config.json` holds per-device settings; `data/config.txt` is an optional fallback Wi-Fi list. The old `data/mqtt.txt` is ignored but is not read by the current MQTT client. A firmware flash and a filesystem upload are different operations: `uploadfs` can replace the device's private SPIFFS contents.

## Build and device workflow on devX

1. Use the private SSH alias `devX` and the checkout at `~/codes/iYak32`. Build with `~/.venvs/iyak32/bin/pio run -e esp32dev`. There is no unit test suite in this repository yet.
2. Confirm the CP210x USB bridge identity before attaching it from Windows to devX with USB/IP. Keep port 3240 on the private LAN and restrict access to devX. The Linux port is `/dev/ttyUSB0` when attached.
3. Read the partition table and active OTA slot before flashing. This board has a 4 MB flash and separate application and SPIFFS partitions. Preserve a full private flash backup outside Git before the first write.
4. Flash only the active application partition when changing firmware; do not upload the filesystem or rewrite the partition table for an ordinary code change. Verify the image hash, check the serial boot log, then run `python3 scripts/smoke_device.py <device-ip>` from devX.
5. If the serial log reports a brownout, stop further flashes and check USB power/cable stability. A successful esptool write proves flash transport, not a successful application boot.

On 2026-10-09, replacing the USB cable changed the CP210x BUSID to `1-8` (serial `01AB9FF2`). The isolated board then booted without brownout, and the application-only flashes completed with esptool hash verification. Serial showed `BOOT done` and `BasicMode: Setup complete`, while the read-only smoke check passed after a reset. This supports a power/cable cause for the earlier brownouts, but does not prove long-term power margin. Check `MAINTENANCE.md` for the latest flash evidence. The changed `data/setup.html` is not on the device until a deliberate SPIFFS update; do not use `uploadfs` with an incomplete private filesystem image.

## Product direction

iYak32 is a small family of ESP32 projects. The first verified board is a room sensor node; repeatable nodes for the garage, basement, and attic remain planned. The next physical build is an indoor iGrow shelf, followed later by a small proximity/camera robot. The kayak controller and nearly finished hockey prototype remain real projects. Keep one shared MQTT/sensor foundation and small, explicit device modes; do not make every board load every feature.

The first isolated board publishes six numeric measurements on `sensors/ESP_15605/<measure>` to the private broker. Its BMP280 does not measure humidity. Home Assistant has accepted its MQTT discovery payload and displayed the sensor device. Each physical board needs a stable unique identity and a friendly room name; `ESP_15605` is the current board's existing identity.

## Indoor iGrow pilot

Start with two or three bins on one shelf: one shared light and fan, one air temperature/humidity measurement, and a soil moisture probe plus watering line per bin. The older iGrow/iGrow32 project and the existing Arduino board and relay module provide hardware clues, but their current wiring, voltages, and sensor models have not been verified. The old pin labels are not a wiring plan.

First inventory and reconnect the hardware, then publish calibrated sensor readings to MQTT/Home Assistant. Test one pump with a locally bounded run time and measured water volume before extending watering to the other bins or enabling automatic rules. A dedicated grow mode can reuse iYak32's MQTT and device discovery without changing the working room node. No grow actuators have been connected or tested on the current board.

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

Home Assistant discovery is accepted for one room node; physical placement and multi-board naming are still open. The indoor shelf is the next hardware pilot. Garden actuators and robot drive controls need their own physical tests. Maintenance tasks and flash evidence live in `MAINTENANCE.md`. Web-page changes require a deliberate SPIFFS update that preserves private files.

On 2026-10-09, Home Assistant discovery was built and flashed to the isolated `ESP_15605` board in active `app0` at `0x10000`. The initial retained payload reached the broker, but Home Assistant rejected its root `~` topic shortcut. Commit `6792584` replaced the shortcut with full state and availability topics. The corrected app image SHA-256 was `fc9a2d21ec45e8463a0a2594dd03c06eecf121381cf64cfb618bc943071e21e0`, and esptool verified the written hash. Serial showed a clean BMP280 boot, MQTT connection, and discovery publication; the HTTP smoke check passed. Home Assistant then created `ESP_15605` as one MQTT device and displayed current temperature and pressure plus battery voltage, Wi-Fi signal, and CPU temperature diagnostics. Humidity is absent because this board has a BMP280.
