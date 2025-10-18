# Peer Review: iYak32, SBQC, DataAPI, and iGrow Interactions

This document provides a peer review of the interactions between the iYak32 firmware and the SBQC, DataAPI, and iGrow backend applications.

## Techno

### iYak32
- **Hardware:** ESP32
- **Framework:** Arduino
- **Build System:** PlatformIO
- **Libraries:**
  - `ESPAsyncWebServer`: For creating a web server.
  - `PubSubClient`: For MQTT communication.
  - `ArduinoJson`: For JSON serialization and deserialization.
  - Various sensor libraries (e.g., `Adafruit_BME280`, `TM1637Display`).

### SBQC & iGrow
- **Backend:** Node.js with Express
- **Database:** MongoDB
- **Communication:** MQTT
- **Frontend:** EJS (Embedded JavaScript templates)

### DataAPI
- **Backend:** Node.js with Express
- **Database:** MongoDB

## Features

### iYak32
- **Web Server:** Provides a web interface for controlling the device and viewing sensor data.
- **MQTT Client:** Communicates with a backend server via MQTT for data logging and remote control.
- **Sensor Integration:** Supports various sensors, including the BME280 (temperature, pressure, humidity) and ACS712 (current).
- **I/O Control:** Allows for generic configuration and real-time monitoring of GPIO pins via a web interface and API.
- **Over-the-Air (OTA) Updates:** Supports firmware updates over Wi-Fi.
- **Multiple Modes:** Can be compiled in different modes (`BOAT`, `HOCKEY_MODE`, `BASIC_MODE`) for different applications.

### SBQC & iGrow
- **Web Interface:** Provides a web interface for viewing device data and managing devices.
- **MQTT Broker/Client:** Acts as an MQTT client to receive data from `iYak32` devices.
- **Device Management:** Registers and tracks the status of `iYak32` devices.
- **Data Forwarding:** Forwards data from `iYak32` devices to the `DataAPI`.

### DataAPI
- **REST API:** Provides a REST API for managing devices, profiles, and sensor data.
- **Data Persistence:** Stores data from `iYak32` devices in a MongoDB database.

## Endpoints

### iYak32

#### HTTP Endpoints
- `GET /api/io/status`: Retrieves the current status of all configured pins.
- `GET /api/io/config`: Retrieves the currently saved `/io_config.json`.
- `POST /api/io/config`: Accepts a new I/O configuration JSON payload.
- `POST /api/io/toggle`: Toggles a digital output pin.
- `POST /config`: Accepts a new device configuration JSON payload.
- `GET /reboot`: Reboots the device.

#### MQTT Topics (Subscribed)
- `esp32/<DEVICE_NAME>/#`: Subscribes to all topics under the device's namespace.

#### MQTT Topics (Published)
- `esp32/register`: Publishes a message to register the device.
- `esp32/config`: Publishes a message to request the device configuration.
- `esp32/data/<DEVICE_NAME>`: Publishes sensor data.
- `esp32/alive/<DEVICE_NAME>`: Publishes heartbeat messages.

### DataAPI

#### API Endpoints
- `GET /api/v1/devices`: Get a list of registered devices.
- `POST /api/v1/devices`: Register a new device.
- `GET /api/v1/profiles`: Get a list of profiles.
- `GET /api/v1/profiles/:name`: Get a specific profile.
- `POST /api/v1/heartbeats`: Save a heartbeat message.

## Interactions

The four applications work together to create a complete IoT system. The interactions can be summarized as follows:

1.  **Device Registration:**
    - The `iYak32` device boots up and connects to the Wi-Fi network.
    - It then publishes a registration message to the `esp32/register` MQTT topic.
    - `SBQC` and `iGrow` are subscribed to this topic. When they receive the message, they call the `DataAPI`'s `/api/v1/devices` endpoint to register the device.

2.  **Configuration Retrieval:**
    - After registration, the `iYak32` device publishes a message to the `esp32/config` topic to request its configuration.
    - `SBQC` and `iGrow` receive this message and call the `DataAPI`'s `/api/v1/profiles/:name` endpoint to retrieve the device's profile.
    - They then publish the configuration to the `esp32/<DEVICE_NAME>/config` topic.
    - **Note:** There is a mismatch here. `iYak32` publishes to `esp32/config`, but `SBQC` listens on `esp32/ioConfig`. `iGrow` listens on `esp32/config`.

3.  **Data Transmission:**
    - The `iYak32` device periodically reads sensor data and publishes it to the `esp32/data/<DEVICE_NAME>` topic.
    - `SBQC` and `iGrow` receive this data and call the `DataAPI`'s `/api/v1/heartbeats` endpoint to save it.

4.  **Connection Monitoring:**
    - The `iYak32` device sends periodic heartbeat messages to the `esp32/alive/<DEVICE_NAME>` topic.
    - `SBQC` and `iGrow` use these messages, along with the data messages, to track the connection status of each device. If a device fails to send a message within a certain time, it is marked as disconnected.

## Instructions

To set up and run the entire system, follow these steps:

1.  **Set up the development environment:**
    - Install Node.js, MongoDB, and Mosquitto on your server.
    - Install PlatformIO on your development machine.

2.  **Clone the repositories:**
    - Clone the `iYak32`, `SBQC`, `DataAPI`, and `iGrow` repositories.

3.  **Configure the applications:**
    - Create `.env` files for `SBQC`, `DataAPI`, and `iGrow` based on the `.env.example` files.
    - Create `esp32config.json` and `io_config.json` files for `iYak32` in the `/data` directory.

4.  **Start the services:**
    - Start the MongoDB and Mosquitto services.
    - Start the `DataAPI`, `SBQC`, and `iGrow` applications.

5.  **Flash the `iYak32` firmware:**
    - Connect the ESP32 to your development machine.
    - Use PlatformIO to build and upload the firmware.

## Pitfalls

- **MQTT Topic Mismatch:** There is a mismatch in the MQTT topic used for configuration requests. The `iYak32` firmware publishes to `esp32/config`, but the `SBQC` application subscribes to `esp32/ioConfig`. This will prevent `iYak32` devices from receiving their configuration when connected to `SBQC`.
- **Code Redundancy:** The `SBQC` and `iGrow` applications are nearly identical. They both perform the same functions of listening for MQTT messages from `iYak32` devices and forwarding the data to the `DataAPI`. This redundancy increases the maintenance overhead and the risk of inconsistencies.
- **Lack of a Unified Backend:** The current architecture uses three separate backend applications (`SBQC`, `iGrow`, and `DataAPI`). This makes the system more complex than necessary and can lead to issues with data consistency and synchronization.
- **No Authentication/Authorization:** The `DataAPI` does not appear to have any authentication or authorization mechanisms in place. This means that anyone with access to the API can read and write data.

## Improvements

- **Consolidate Backend Services:** The `SBQC` and `iGrow` applications should be merged into a single application. This would reduce code redundancy and simplify the architecture. Ideally, the MQTT handling logic would be integrated directly into the `DataAPI`, creating a single, unified backend service.
- **Fix MQTT Topic Mismatch:** The MQTT topic for configuration requests should be consistent across all applications. The `iYak32` firmware and the `SBQC` application should be updated to use the same topic (e.g., `esp32/config`).
- **Implement Authentication and Authorization:** The `DataAPI` should be secured with an authentication and authorization mechanism (e.g., JWTs or OAuth) to control access to the API.
- **Standardize Configuration:** The configuration process for all applications should be standardized. This includes using a consistent format for `.env` files and a clear process for managing configuration variables.