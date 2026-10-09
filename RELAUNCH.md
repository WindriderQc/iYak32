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

The first devX application-only flash completed and esptool verified its hash. Runtime acceptance is pending because the isolated board repeatedly reported a brownout before `BasicMode` finished setup. The same failure occurred over USB/IP and native Windows COM3, so the tunnel is not required to reproduce it. Later code commits have only passed the PlatformIO build. The changed `data/setup.html` is not on the device until a deliberate SPIFFS update; do not use `uploadfs` with an incomplete private filesystem image.

## Top 25 practical improvements

Ordered by risk and usefulness. "Built" means the source compiles; it does not claim device acceptance.

| # | Improvement | State |
|---:|---|---|
| 1 | Stabilize USB power and explain the observed boot brownout before more flashing. | Hardware check pending |
| 2 | Stop serving private config files and credentials through HTTP responses and serial logs. | Partly built; tide-key route still open |
| 3 | Require an owner credential for HTTP writes, reboot, and OTA; keep it LAN-only. | Open |
| 4 | Protect MQTT credentials in transit or move the broker onto the private LAN. | Open |
| 5 | Add a safe stop/interlock and correct ADC-to-8-bit PWM scaling in `BOAT`. | Open; mode disabled |
| 6 | Never auto-format SPIFFS on mount failure. | Built; device check pending |
| 7 | Never delete a config file just because opening it failed. | Built; device check pending |
| 8 | Save config with a temporary file and replace it only after a complete write. | Open |
| 9 | Give each HTTP request its own bounded body buffer; reject oversized input. | Built; device check pending |
| 10 | Accept exact device MQTT topics and drive configured digital outputs only. | Built; device check pending |
| 11 | Validate MQTT `configIOs` and web GPIO settings before touching pins, including reserved and duplicate pins. | Open |
| 12 | Pin PlatformIO platform and library versions so Windows and devX build the same image. | Done; devX rebuild matched flashed image SHA-256 |
| 13 | Keep a private full-flash backup and a tested restore procedure. | Backup done; restore drill pending |
| 14 | Reconcile Button3 GPIO 27 in `BasicMode` with GPIO 34 in tracked `data/io_config.json`. | Wiring check needed |
| 15 | Reject pull-up/down and output modes on GPIO 34–39; include GPIO 39 in the pin table. | Built; device check pending |
| 16 | Identify whether the device at I2C `0x76` is BMP280 or BME280 and select the right driver. | Hardware check needed |
| 17 | Represent missing sensor readings as unavailable instead of valid zeroes. | Done for sensor history; live device returned null for missing BME280 |
| 18 | Produce actual UTC ISO 8601 timestamps for MQTT; keep local time for the UI. | Built; device check pending |
| 19 | Check MQTT publish results before removing queued messages. | Built; device check pending |
| 20 | Initialize `BasicMode` LED outputs without relying on SPIFFS GPIO config. | Flashed; boot marker pending |
| 21 | Let ArduinoOTA register mDNS only once; check discovery on a stable device. | Built; device check pending |
| 22 | Require an explicit OTA target instead of a hard-coded IP. | Built |
| 23 | Make the hockey score writer and stats page use the same SPIFFS path. | Open; mode disabled |
| 24 | Bundle required web assets locally so the UI works without Internet access. | Open |
| 25 | Add a small build gate and hardware smoke checklist for boot, GPIO, web, MQTT, and mode selection. | Read-only BasicMode smoke script added and passed on device; serial and other modes remain manual |

The older `PEER_REVIEW.md` contains stale claims (for example, it calls the buzzer blocking). Check current source and serial evidence before using it as a task list.
