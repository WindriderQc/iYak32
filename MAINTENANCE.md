# iYak32 maintenance backlog

These are the relaunch, reliability, and safety tasks. Product features are prioritized in `RELAUNCH.md`.

## Technical tasks and device evidence

Ordered by risk and usefulness. "Built" means the source compiles; it does not claim device acceptance.

| # | Improvement | State |
|---:|---|---|
| 1 | Stabilize USB power and explain the observed boot brownout before more flashing. | New cable: repeated clean boots and verified flashes; long-term margin untested |
| 2 | Stop serving private config files and credentials through HTTP responses and serial logs. | Partly built; tide-key route still open |
| 3 | Require an owner credential for HTTP writes, reboot, and OTA; keep it LAN-only. | Open |
| 4 | Protect MQTT credentials in transit or move the broker onto the private LAN. | Moved to LAN broker; device connected and six topics received; credentials private |
| 5 | Add a safe stop/interlock and correct ADC-to-8-bit PWM scaling in `BOAT`. | Open; mode disabled |
| 6 | Never auto-format SPIFFS on mount failure. | Built; device check pending |
| 7 | Never delete a config file just because opening it failed. | Built; device check pending |
| 8 | Save config with a temporary file and replace it only after a complete write. | Open |
| 9 | Give each HTTP request its own bounded body buffer; reject oversized input. | Live oversized JSON returned HTTP 413 |
| 10 | Accept exact device MQTT topics and drive configured digital outputs only. | Built; device check pending |
| 11 | Validate MQTT `configIOs` and web GPIO settings before touching pins, including reserved and duplicate pins. | Open |
| 12 | Pin PlatformIO platform and library versions so Windows and devX build the same image. | Done; devX rebuild matched flashed image SHA-256 |
| 13 | Keep a private full-flash backup and a tested restore procedure. | Backup done; restore drill pending |
| 14 | Reconcile Button3 GPIO 27 in `BasicMode` with GPIO 34 in tracked `data/io_config.json`. | Wiring check needed |
| 15 | Reject pull-up/down and output modes on GPIO 34–39; include GPIO 39 in the pin table. | Built; device check pending |
| 16 | Identify whether the device at I2C `0x76` is BMP280 or BME280 and select the right driver. | Done; chip ID `0x58` is BMP280, live temperature and pressure confirmed |
| 17 | Represent missing sensor readings as unavailable instead of valid zeroes. | Done; BMP280 humidity is null while temperature and pressure are live |
| 18 | Produce actual UTC ISO 8601 timestamps for MQTT; keep local time for the UI. | Built; device check pending |
| 19 | Check MQTT publish results before removing queued messages. | Built; device check pending |
| 20 | Initialize `BasicMode` LED outputs without relying on SPIFFS GPIO config. | Flashed; serial boot marker and HTTP status confirmed |
| 21 | Let ArduinoOTA register mDNS only once; check discovery on a stable device. | No duplicate-service error on new firmware; discovery untested |
| 22 | Require an explicit OTA target instead of a hard-coded IP. | Built |
| 23 | Make the hockey score writer and stats page use the same SPIFFS path. | Open; mode disabled |
| 24 | Bundle required web assets locally so the UI works without Internet access. | Open |
| 25 | Add a small build gate and hardware smoke checklist for boot, GPIO, web, MQTT, and mode selection. | Read-only BasicMode smoke script added and passed on device; serial and other modes remain manual |

The older `PEER_REVIEW.md` contains stale claims (for example, it calls the buzzer blocking). Check current source and serial evidence before using it as a task list.

Live follow-up: Wi-Fi RSSI averaging used unsigned division and reported `429496679` instead of a negative dBm value. Commit `bac92a8` was flashed; `/api/wifi/status`, `/api/status`, and sensor history then reported `-77 dBm`.
