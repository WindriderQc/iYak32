# iYak32

Kayak ESP32 controller with 270 deg servo, BTS7960 PWM to control electric motor direction and speed, controls available on physical controller via 2 potentiometers and BCK/FWD switch.

This project is to control a Minn Kota 30 trolling motor at the back of an OldTown Predator 13 fishing kayak.



-------------------------------------------------------------------------------



Available features:
- BMP280/BME280 supported on I2C channel
- OLED display supported on I2C channel to display wifi IP address, speed, direction and atmospherical pressure
- Async web server to provide interface with atmospheric data and controls in addition to physical controller
- WifiManager trying to connect to a list of preferred SSID, launching access Point server if no wifi connections are available.  
    a) config.txt file must be added to /data folder to list your preferred SSID:password 
    b) numPreferredNetworks in WifiManager.h must be adjusted accordingly to number of lines/SSID in config.txt
- ACS712 Current sensor library implemented
- 4 digits 7-segments Display library based on TM1637Display to show String, Char and Int implemented  
- Over The Air programming capabilities with platform.io implemented
    a) ESP IP address must be configured in platformio.ini and ESP must be connected a preferred wifi to be on the same network as the intended programming computer.  A first USB connection is obviously required  
- **Hockey Scoreboard Application:**
    - Implements a fully functional air hockey or table hockey scoreboard.
    - Utilizes a TM1637 4-digit 7-segment display (via `SevenSegmentAscii.h`) to show game time and scores.
    - Features goal detection using analog light sensors (`AnLux.h`) connected to analog pins.
    - Manages game state including period tracking, game timer (configurable length), pause/reset functionality.
    - Includes basic sounds for goals/period end via the buzzer.
    - Saves game scores with timestamps to a file (`game_scores.txt`) on the ESP32's filesystem.
    - Provides a web interface component under `/hockey/Scoreboard.html` for viewing scores (though primary display is the 7-segment).

## Generic I/O Configuration Web Interface

The iYak32 firmware includes a powerful web interface that allows for generic configuration and real-time status monitoring of the ESP32's GPIO pins. This feature makes it easy to adapt the ESP32 for various custom projects without needing to recompile the firmware for basic I/O changes.

You can access this interface via the `io_control.html` page on the ESP32's web server (e.g., `http://<ESP32_IP>/io_control.html`).

**Key Functionalities:**

*   **Dynamic Status Display:** View the current state (HIGH/LOW or analog value) of configured I/O pins, updated periodically.
*   **Pin Configuration:** For a list of available GPIOs, you can:
    *   Assign custom **labels** (names).
    *   Set the **mode:** `OUTPUT`, `INPUT`, `INPUT_PULLUP`, `INPUT_PULLDOWN`.
    *   Define the **type:** `DIGITAL` or `ANALOG_INPUT`.
    *   Specify an **initial state** for output pins (`LOW` or `HIGH`).
*   **Configuration Management:**
    *   Save the entire I/O pin configuration to the ESP32. These settings are stored in `/io_config.json` on the device's SPIFFS filesystem and are applied at boot.
    *   View the current configuration in JSON format directly in the UI.
*   **Real-time Graphing:** Select pins to visualize their status over time on a line graph, powered by Chart.js.

**I/O Configuration JSON Structure:**

The I/O settings are stored in a JSON file named `/io_config.json`. The structure is as follows:

```json
{
  "io_pins": [
    {
      "gpio": 21,
      "label": "Builtin LED",
      "mode": "OUTPUT",
      "type": "DIGITAL",
      "initial_state": "LOW",
      "graph": false
    },
    {
      "gpio": 34,
      "label": "Light Sensor",
      "mode": "INPUT",
      "type": "ANALOG_INPUT",
      "graph": true
    }
  ]
}
```
Each object in the `io_pins` array defines a single GPIO pin's configuration.

**Supporting API Endpoints:**

The web interface interacts with the ESP32 through the following HTTP API endpoints:

*   `GET /api/io/status`: Retrieves the current status (values) of all configured pins.
*   `GET /api/io/config`: Retrieves the currently saved `/io_config.json`.
*   `POST /api/io/config`: Accepts a new I/O configuration JSON payload, saves it to `/io_config.json`, and applies the settings to the ESP32's GPIOs.



 // Set the SSID (name) and password of the Access Point
    const char* apSsid = "iYak32";
    const char* apPassword = "12345678";
-------------------------------------------------------------------------------

- MQTT
    mqtt.txt  file must be in the data folder with a simple line:
user:password



Upcoming intentions:
- Add directions and speed control from webInterface
- Autolock button on webInterface to Use cellphone GPS via the Javascript within the webServer to send auto command to motor to stay in position 
- Add SEALEVELPRESSURE_HPA variable on webInterface to allow for proper altitude detection.  Possibly get it from meteorological API if internet access is available otherwise fall back on manual entry
