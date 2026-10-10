# iYak32

PlatformIO/Arduino firmware for a small family of ESP32 projects. The active build runs `BASIC_MODE` as a local sensor node. The next physical project is an indoor iGrow shelf with two or three plant bins. The kayak controller and hockey scoreboard remain separate, unfinished modes.

- [RELAUNCH.md](RELAUNCH.md) explains the code, device workflow, and product roadmap.
- [MAINTENANCE.md](MAINTENANCE.md) tracks technical work and distinguishes built code from real-device checks.

## Verified sensor node

On 2026-10-09, the isolated board `ESP_15605` booted cleanly after an application-only flash. Its BMP280 sent temperature and pressure to the private MQTT broker, and Home Assistant created one MQTT device with those readings and battery, Wi-Fi, and CPU diagnostics. A BMP280 cannot measure humidity. The board's room placement and a second node have not been tested yet.

`src/main.cpp` enables `BASIC_MODE` at compile time. The `BOAT` and `HOCKEY_MODE` definitions are disabled. The firmware mounts SPIFFS, loads private configuration, starts Wi-Fi/MQTT/HTTP, and publishes telemetry. Files in `data/` are served by the ESP32 web server.

## Build and configuration

Build on the active devX checkout with `~/.venvs/iyak32/bin/pio run -e esp32dev`. There is no unit test suite yet. `scripts/smoke_device.py` performs a read-only HTTP check against a reachable board.

Copy `data/esp32config.example.json` to the gitignored `data/esp32config.json` for device-specific settings. Set `ssid`/`pass`, `mqtturl`/`mqttport`, and `mqttuser`/`mqttpass` there. The optional gitignored `data/config.txt`, based on `data/config.example.txt`, lists fallback Wi-Fi networks as `SSID:PASSWORD`. The old `mqtt.txt` file is not used by the current MQTT client.

Keep passwords and per-device files outside Git. An application flash and a SPIFFS filesystem upload are different operations: `uploadfs` can replace private files already on the board. The OTA environment requires an explicit device target. See [RELAUNCH.md](RELAUNCH.md) for the verified USB/IP and flash workflow.

## MQTT and Home Assistant

The JSON envelope is published on `esp32/data/<DEVICE_NAME>`; scalar readings are published on `sensors/<DEVICE_NAME>/<measure>` every `mqttDataIntervalSec` seconds. The envelope includes a UTC timestamp, sender, message type, and payload.

| Measure | Unit | Source |
|---|---|---|
| `temperature` | °C | Detected BMP280/BME280 |
| `pressure` | hPa | Detected BMP280/BME280 |
| `altitude` | m | Pressure estimate |
| `humidity` | % | BME280 only |
| `battery_voltage` | V | Battery ADC estimate |
| `wifi_rssi` | dBm | Wi-Fi |
| `cpu_temperature` | °C | ESP32 |

The firmware publishes retained discovery to `homeassistant/device/<DEVICE_NAME>/config` and retained availability to `sensors/<DEVICE_NAME>/availability`. Add Home Assistant's MQTT integration with the same private broker and leave discovery enabled. It groups the available readings into one device. Assign a friendly name and area in Home Assistant when the physical location is known. The pressure-derived altitude stays on MQTT but is omitted from the HA room device.

## Local UI and code map

The ESP32 serves a local UI from SPIFFS. `/io_control.html` displays configured GPIO values and uses `GET /api/io/status`, `GET /api/io/config`, and `POST /api/io/config`. The I/O configuration is stored in `/io_config.json`. Check the actual board wiring before changing GPIO roles; the general I/O API is not yet the controller for pumps or lights.

- `src/api/Esp32.cpp`: boot, configuration, GPIO setup, and shared services.
- `src/api/WifiManager.cpp`: Wi-Fi fallback and OTA.
- `src/api/Mqtt.cpp` and `MqttCommandRouter.cpp`: telemetry, discovery, reconnect, and commands.
- `src/www.h`: HTTP routes; `data/`: SPIFFS web assets.
- `src/api/devices/`: BMP280/BME280, display, and buzzer.
- `src/BasicMode.h`, `src/Boat.h`, `src/Hockey.h`: compile-time application modes.

## Next: indoor iGrow

The agreed pilot is one shelf with two or three bins, a shared light and fan, shared air temperature/humidity sensing, and soil moisture plus watering for each bin. The original iGrow/iGrow32 hardware includes an Arduino board and a relay module; its wiring and component voltages still need to be identified. First bring sensor readings into HA, then test one bounded pump run and measure delivered water before extending control to every bin. The legacy iGrow and iGrow32 code are references, not the active firmware.

The original kayak design used a servo and BTS7960 motor driver for a Minn Kota trolling motor. The hockey prototype has a TM1637 display, goal sensors, timing, and sounds. Both need their own hardware acceptance before being treated as working modes.
