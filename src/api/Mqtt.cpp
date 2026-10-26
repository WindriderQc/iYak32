#include "Mqtt.h"
#include "api/Esp32.h"    // For Esp32::hourglass and Esp32::DEVICE_NAME
#include "api/SystemLog.h" // For error logging
#include "IODiscovery.h"
#include "IOConfig.h"
#include "Storage.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <cmath>
#include <freertos/semphr.h>

namespace Mqtt {
    bool isEnabled = false;
    String serverIp = "";
    int port = 1883;
    String mqttUser = "";
    String mqttPass = "";
    WiFiClient client;
    PubSubClient mqttClient(client);
    MqttQueue mqttQueue;

    // Connection state
    MqttState currentState_ = MqttState::OFF;
    unsigned long lastConnectAttempt_ = 0;
    int connectAttemptCount_ = 0;
    int backoffMultiplier_ = 0;
    unsigned long backoffDuration_ = BACKOFF_BASE_MS;
    String pendingDeviceName_ = "";
    static bool discoveryPublished_ = false;
    bool ioDiscoveryReady = false;
    unsigned int ioDiscoveryCount = 0;
    static String ioDiscoveryRevision;
    static String ioPublishedDomains[HUZZAH32];
    static bool ioDomainKnown[HUZZAH32] = {};
    unsigned int automationDiscoveryCount = 0;
    static JsonDocument publishedScenes;
    static bool sceneIDsLoaded = false;

    void requestHomeAssistantDiscovery() {
        discoveryPublished_ = false;
        ioDiscoveryRevision = "";
        ioDiscoveryReady = false;
    }
    static volatile bool reconnectRequested_ = false;

    // Keep one bounded preview per topic, so heartbeats do not bury sensor data.
    struct TrafficEntry {
        String topic;
        String payload;
        unsigned long at = 0;
        bool incoming = false;
        bool ok = true;
        bool retained = false;
        bool truncated = false;
    };
    static TrafficEntry traffic_[16];
    static unsigned int trafficNext_ = 0;
    static unsigned long sent_ = 0, received_ = 0, failed_ = 0;
    static SemaphoreHandle_t trafficMutex_ = xSemaphoreCreateMutex();

    static void recordTraffic(const char* topic, const byte* payload, unsigned int length,
                              bool incoming, bool ok, bool retained) {
        if (!trafficMutex_ || xSemaphoreTake(trafficMutex_, pdMS_TO_TICKS(20)) != pdTRUE) return;
        if (incoming) ++received_;
        else if (ok) ++sent_;
        else ++failed_;
        unsigned int slot = trafficNext_;
        bool found = false;
        for (unsigned int i = 0; i < 16; ++i) {
            if (traffic_[i].topic == topic && traffic_[i].incoming == incoming) {
                slot = i;
                found = true;
                break;
            }
        }
        if (!found) trafficNext_ = (trafficNext_ + 1) % 16;
        TrafficEntry& entry = traffic_[slot];
        entry.topic = topic;
        entry.payload = "";
        const unsigned int previewLength = min(length, 512U);
        entry.payload.reserve(previewLength);
        for (unsigned int i = 0; i < previewLength; ++i) entry.payload += static_cast<char>(payload[i]);
        entry.at = millis();
        entry.incoming = incoming;
        entry.ok = ok;
        entry.retained = retained;
        entry.truncated = length > previewLength;
        xSemaphoreGive(trafficMutex_);
    }

    bool publish(const char* topic, const char* payload, bool retained) {
        const bool ok = mqttClient.publish(topic, payload, retained);
        recordTraffic(topic, reinterpret_cast<const byte*>(payload), strlen(payload), false, ok, retained);
        return ok;
    }

    void recordIncoming(const char* topic, const byte* payload, unsigned int length) {
        recordTraffic(topic, payload, length, true, true, false);
    }

    String getTrafficJson() {
        JsonDocument doc;
        JsonArray topics = doc["topics"].to<JsonArray>();
        if (!trafficMutex_ || xSemaphoreTake(trafficMutex_, pdMS_TO_TICKS(50)) != pdTRUE) return "{}";
        doc["sent"] = sent_;
        doc["received"] = received_;
        doc["failed"] = failed_;
        for (const TrafficEntry& entry : traffic_) {
            if (entry.topic.isEmpty()) continue;
            JsonObject item = topics.add<JsonObject>();
            item["topic"] = entry.topic;
            item["payload"] = entry.payload;
            item["age_ms"] = millis() - entry.at;
            item["direction"] = entry.incoming ? "in" : "out";
            item["ok"] = entry.ok;
            item["retained"] = entry.retained;
            item["truncated"] = entry.truncated;
        }
        xSemaphoreGive(trafficMutex_);
        String response;
        serializeJson(doc, response);
        return response;
    }

    // State machine API
    MqttState getState() { return currentState_; }

    const char* getStateString() {
        switch (currentState_) {
            case MqttState::OFF:      return "disabled";
            case MqttState::DISCONNECTED: return "disconnected";
            case MqttState::CONNECTING:   return "connecting";
            case MqttState::CONNECTED:    return "connected";
            case MqttState::BACKOFF:      return "backoff";
            default:                      return "unknown";
        }
    }

    void forceReconnect() {
        // HTTP runs on another task. Let the main loop own the MQTT client.
        if (isEnabled) reconnectRequested_ = true;
    }

    // Function Definitions
    MqttMsg sliceMqttMsg(char* topic, byte* message, unsigned int length) {
        MqttMsg mqttMsg;
        size_t input_topic_len = strlen(topic);
        size_t max_topic_buf_len = sizeof(mqttMsg.topicChar) - 1;
        size_t chars_to_copy_topic = input_topic_len;

        if (input_topic_len >= sizeof(mqttMsg.topicChar)) {
            Serial.println(F("ERROR: MQTT topic too long, truncating."));
            chars_to_copy_topic = max_topic_buf_len;
            mqttMsg.topicTruncated = true;
        }
        strncpy(mqttMsg.topicChar, topic, chars_to_copy_topic);
        mqttMsg.topicChar[chars_to_copy_topic] = '\0';

        mqttMsg.topicStr = "";
        for (size_t k = 0; k < chars_to_copy_topic; ++k) {
            mqttMsg.topicStr += mqttMsg.topicChar[k];
        }

        size_t max_msg_buf_len = sizeof(mqttMsg.msgArray) - 1;
        size_t chars_to_copy_msg = length;

        if (length >= sizeof(mqttMsg.msgArray)) {
            Serial.println(F("ERROR: MQTT message payload too long, truncating."));
            chars_to_copy_msg = max_msg_buf_len;
            mqttMsg.msgTruncated = true;
        }

        mqttMsg.msgStr = "";
        for (unsigned int k = 0; k < chars_to_copy_msg; ++k) {
            mqttMsg.msgArray[k] = (char)message[k];
            mqttMsg.msgStr += (char)message[k];
        }
        mqttMsg.msgArray[chars_to_copy_msg] = '\0';

        memset(mqttMsg.topicTokens, 0, sizeof(mqttMsg.topicTokens));
        memset(mqttMsg.msgTokens, 0, sizeof(mqttMsg.msgTokens));

        char *ptr = NULL;
        byte index = 0;
        ptr = strtok(mqttMsg.topicChar, "/");
        while(ptr != NULL && index < MAX_TOKEN) {
            mqttMsg.topicTokens[index] = ptr;  index++;  ptr = strtok(NULL, "/");
        }

        index = 0;
        ptr = strtok(mqttMsg.msgArray, ":");
        while(ptr != NULL && index < MAX_TOKEN) {
            mqttMsg.msgTokens[index] = ptr;  index++;  ptr = strtok(NULL, ":");
        }
        return mqttMsg;
    }

    void subscription(String deviceName) {
        mqttClient.subscribe("esp32");
        mqttClient.subscribe("homeassistant/status");
        mqttClient.subscribe(String("esp32/" + deviceName + "/#").c_str());
    }

    bool setup(String deviceName, const JsonDocument& config) {
        auto sanitize = [](String value) -> String {
            value.trim();
            if (value.equalsIgnoreCase("null") || value.equalsIgnoreCase("undefined")) {
                return "";
            }
            return value;
        };

        // Canonical keys first, deprecated fallbacks second
        serverIp = sanitize(config["mqtturl"].as<String>());
        if (serverIp.isEmpty()) {
            serverIp = sanitize(config["mqtt_server"].as<String>());
            if (!serverIp.isEmpty()) Serial.println(F("MQTT WARN: Using deprecated 'mqtt_server' key. Migrate to 'mqtturl'."));
        }
        if (serverIp.isEmpty()) {
            String ip0 = sanitize(config["ip0"].as<String>());
            String ip1 = sanitize(config["ip1"].as<String>());
            String ip2 = sanitize(config["ip2"].as<String>());
            String ip3 = sanitize(config["ip3"].as<String>());
            if (!ip0.isEmpty() && !ip1.isEmpty() && !ip2.isEmpty() && !ip3.isEmpty()) {
                serverIp = ip0 + "." + ip1 + "." + ip2 + "." + ip3;
                Serial.println(F("MQTT WARN: Using deprecated ip0-ip3 keys. Migrate to 'mqtturl'."));
            }
        }

        int configuredPort = config["mqttport"] | 0;
        if (configuredPort <= 0) {
            configuredPort = config["mqtt_port"] | 0;
            if (configuredPort > 0) Serial.println(F("MQTT WARN: Using deprecated 'mqtt_port' key. Migrate to 'mqttport'."));
        }
        port = configuredPort > 0 ? configuredPort : 1883;

        mqttUser = sanitize(config["mqttuser"].as<String>());
        if (mqttUser.isEmpty()) {
            mqttUser = sanitize(config["mqtt_user"].as<String>());
            if (!mqttUser.isEmpty()) Serial.println(F("MQTT WARN: Using deprecated 'mqtt_user' key. Migrate to 'mqttuser'."));
        }

        mqttPass = sanitize(config["mqttpass"].as<String>());
        if (mqttPass.isEmpty()) {
            mqttPass = sanitize(config["mqtt_pass"].as<String>());
            if (!mqttPass.isEmpty()) Serial.println(F("MQTT WARN: Using deprecated 'mqtt_pass' key. Migrate to 'mqttpass'."));
        }

        if (serverIp.isEmpty()) {
            Serial.println(F("MQTT server not configured (expected 'mqtturl' in esp32config.json)"));
            currentState_ = MqttState::OFF;
            return false;
        }

        Serial.print(F("MQTT configured: ")); Serial.print(serverIp);
        Serial.print(F(":")); Serial.println(port);

        mqttClient.disconnect();
        mqttClient.setServer(serverIp.c_str(), port);

        // Start non-blocking connection attempts
        pendingDeviceName_ = deviceName;
        connectAttemptCount_ = 0;
        backoffMultiplier_ = 0;
        backoffDuration_ = BACKOFF_BASE_MS;
        lastConnectAttempt_ = 0;
        currentState_ = MqttState::CONNECTING;

        SystemLog::logError("MQTT: Setup initiated", 0);
        return true;
    }

    static bool tryConnect() {
        if (mqttClient.connected()) {
            if (currentState_ != MqttState::CONNECTED) {
                currentState_ = MqttState::CONNECTED;
                Serial.println(F("MQTT: State -> CONNECTED"));
                SystemLog::logError("MQTT: Connected to broker", 0);
            }
            return true;
        }

        if (connectAttemptCount_ >= MAX_ATTEMPTS_PER_BURST) {
            // Transition to backoff with exponential duration
            backoffMultiplier_++;
            backoffDuration_ = BACKOFF_BASE_MS * (1UL << min(backoffMultiplier_ - 1, 4));
            if (backoffDuration_ > BACKOFF_MAX_MS) backoffDuration_ = BACKOFF_MAX_MS;

            Serial.printf("MQTT: Max burst attempts reached. Backing off for %lu ms.\n", backoffDuration_);
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "MQTT: Backoff %lums (burst #%d)", backoffDuration_, backoffMultiplier_);
            SystemLog::logError(logMsg, 1);

            lastConnectAttempt_ = millis();
            connectAttemptCount_ = 0;
            currentState_ = MqttState::BACKOFF;
            return false;
        }

        if (lastConnectAttempt_ != 0 && (millis() - lastConnectAttempt_) < BACKOFF_BASE_MS) {
            return false; // Not time yet between attempts within a burst
        }

        lastConnectAttempt_ = millis();
        connectAttemptCount_++;

        char clientId[32];
        snprintf(clientId, sizeof(clientId), "iYak32-%012llX", static_cast<unsigned long long>(ESP.getEfuseMac()));
        const String availabilityTopic = "sensors/" + pendingDeviceName_ + "/availability";
        Serial.printf("MQTT: Attempt %d/%d... ", connectAttemptCount_, MAX_ATTEMPTS_PER_BURST);

        if (mqttClient.connect(clientId, mqttUser.c_str(), mqttPass.c_str(),
                               availabilityTopic.c_str(), 0, true, "offline")) {
            Serial.println(F("connected"));
            publish("esp32", String("hello from " + pendingDeviceName_).c_str());
            subscription(pendingDeviceName_);
            requestHomeAssistantDiscovery();
            currentState_ = MqttState::CONNECTED;
            connectAttemptCount_ = 0;
            backoffMultiplier_ = 0;
            backoffDuration_ = BACKOFF_BASE_MS;
            SystemLog::logError("MQTT: Connected to broker", 0);
            return true;
        } else {
            Serial.printf("failed, rc=%d\n", mqttClient.state());
            return false;
        }
    }

    void loop() {
        if (reconnectRequested_) {
            reconnectRequested_ = false;
            mqttClient.disconnect();
            requestHomeAssistantDiscovery();
            Serial.println(F("MQTT: Force reconnect requested."));
            SystemLog::logError("MQTT: Force reconnect requested", 0);
            connectAttemptCount_ = 0;
            backoffMultiplier_ = 0;
            backoffDuration_ = BACKOFF_BASE_MS;
            lastConnectAttempt_ = 0;
            currentState_ = MqttState::CONNECTING;
        }
        switch (currentState_) {
            case MqttState::OFF:
                break;

            case MqttState::CONNECTING:
                tryConnect();
                break;

            case MqttState::CONNECTED:
                if (mqttClient.connected()) {
                    mqttClient.loop();
                    mqttQueue.publish();
                } else {
                    Serial.println(F("MQTT: Connection lost!"));
                    SystemLog::logError("MQTT: Connection lost", 2);
                    requestHomeAssistantDiscovery();
                    connectAttemptCount_ = 0;
                    lastConnectAttempt_ = 0;
                    currentState_ = MqttState::CONNECTING;
                }
                break;

            case MqttState::BACKOFF:
                if (millis() - lastConnectAttempt_ >= backoffDuration_) {
                    Serial.println(F("MQTT: Backoff complete, retrying..."));
                    connectAttemptCount_ = 0;
                    lastConnectAttempt_ = 0;
                    currentState_ = MqttState::CONNECTING;
                }
                break;

            case MqttState::DISCONNECTED:
                // Waiting for external trigger (forceReconnect)
                break;
        }
    }

    void publishSensorReading(const char* measure, float value, unsigned int decimals) {
        if (!isEnabled || !mqttClient.connected() || !std::isfinite(value)) return;
        const String topic = "sensors/" + Esp32::DEVICE_NAME + "/" + measure;
        const String payload(value, decimals);
        if (!publish(topic.c_str(), payload.c_str())) {
            Serial.print(F("MQTT: Sensor publish failed on "));
            Serial.println(topic);
        }
    }

    void publishHomeAssistantDiscovery(bool hasClimateSensor, bool hasHumidity) {
        static String publishedName;
        String friendly = Esp32::getFriendlyName();
        if (!isEnabled || !mqttClient.connected() || (discoveryPublished_ && publishedName == friendly)) return;

        const String baseTopic = "sensors/" + Esp32::DEVICE_NAME;
        const String configTopic = "homeassistant/device/" + Esp32::DEVICE_NAME + "/config";
        const String availabilityTopic = baseTopic + "/availability";

        JsonDocument doc;
        JsonObject device = doc["device"].to<JsonObject>();
        device["identifiers"].to<JsonArray>().add(Esp32::DEVICE_NAME);
        device["name"] = friendly;
        device["manufacturer"] = "iYak32";
        device["model"] = "ESP32 sensor node";
        doc["origin"]["name"] = "iYak32";
        doc["availability_topic"] = availabilityTopic;

        JsonObject components = doc["components"].to<JsonObject>();
        auto addSensor = [&](const char* measure, const char* name, const char* deviceClass,
                             const char* unit, bool diagnostic) {
            JsonObject sensor = components[measure].to<JsonObject>();
            sensor["platform"] = "sensor";
            sensor["unique_id"] = Esp32::DEVICE_NAME + "_" + measure;
            sensor["name"] = name;
            sensor["device_class"] = deviceClass;
            sensor["state_class"] = "measurement";
            sensor["unit_of_measurement"] = unit;
            sensor["state_topic"] = baseTopic + "/" + measure;
            if (diagnostic) sensor["entity_category"] = "diagnostic";
        };

        if (hasClimateSensor) {
            addSensor("temperature", "Temperature", "temperature", "°C", false);
            addSensor("pressure", "Pressure", "atmospheric_pressure", "hPa", false);
            if (hasHumidity) addSensor("humidity", "Humidity", "humidity", "%", false);
        }
        addSensor("battery_voltage", "Battery voltage", "voltage", "V", true);
        addSensor("wifi_rssi", "Wi-Fi signal", "signal_strength", "dBm", true);
        addSensor("cpu_temperature", "CPU temperature", "temperature", "°C", true);

        char payload[MQTT_MAX_PACKET_SIZE];
        const size_t payloadLength = serializeJson(doc, payload, sizeof(payload));
        if (payloadLength == 0 || payloadLength >= sizeof(payload) - 1) {
            Serial.println(F("MQTT: Home Assistant discovery payload is too large."));
            return;
        }
        if (!publish(configTopic.c_str(), payload, true) ||
            !publish(availabilityTopic.c_str(), "online", true)) {
            Serial.println(F("MQTT: Home Assistant discovery publish failed; will retry."));
            return;
        }
        discoveryPublished_ = true;
        publishedName = friendly;
        Serial.println(F("MQTT: Home Assistant discovery published."));
    }

    void publishIOHomeAssistantDiscovery(const JsonDocument& status) {
        if (!isEnabled || !mqttClient.connected()) return;
        const String revision = status["_revision"] | "";
        JsonDocument sceneConfig; deserializeJson(sceneConfig, Esp32::getIOConfigurationJsonString());
        bool sceneStateOK = true;
        for (JsonVariantConst scene : sceneConfig["scenes"].as<JsonArrayConst>()) {
            bool available = true;
            for (JsonVariantConst action : scene["actions"].as<JsonArrayConst>()) {
                bool found = false;
                for (JsonVariantConst pin : status["statuses"].as<JsonArrayConst>()) if (pin["gpio"] == action["gpio"]) found = pin["value"].is<double>();
                if (!found) available = false;
            }
            if (!publish(String("esp32/" + Esp32::DEVICE_NAME + "/io/scene/" + scene["id"].as<String>() + "/state").c_str(), available ? "{\"available\":true}" : "{\"available\":false}", true)) sceneStateOK = false;
        }
        if (sceneStateOK && ioDiscoveryReady && ioDiscoveryRevision == revision) return;
        unsigned int count = 0;
        bool ok = sceneStateOK;
        for (int gpio = 0; gpio < HUZZAH32; ++gpio) {
            if (!IOConfig::available(gpio)) continue;
            JsonObjectConst configured;
            for (JsonObjectConst pin : status["statuses"].as<JsonArrayConst>())
                if (pin["gpio"].as<int>() == gpio) { configured = pin; break; }
            String nextDomain = configured.isNull() ? "" : IODiscovery::domain(configured["mode"] | "INPUT", configured["type"] | "DIGITAL");
            bool pinOk = true;
            // On boot clear stale domains, including pins removed while offline.
            for (const char* candidate : {"switch", "binary_sensor", "sensor"}) {
                if (nextDomain == candidate || (ioDomainKnown[gpio] && ioPublishedDomains[gpio] != candidate)) continue;
                String topic = "homeassistant/" + String(candidate) + "/" + Esp32::DEVICE_NAME + "_gpio" + String(gpio) + "/config";
                if (!publish(topic.c_str(), "", true)) pinOk = false;
            }
            if (!nextDomain.isEmpty()) {
                JsonDocument doc;
                String configUrl = "http://" + Esp32::wifiManager.getIPString() + "/io_control.html";
                String friendly = Esp32::getFriendlyName();
                IODiscovery::build(doc, Esp32::DEVICE_NAME.c_str(), gpio, configured["label"] | "GPIO", nextDomain.c_str(), configUrl.c_str(), configured["calibration"], friendly.c_str(), configured["icon"] | "");
                String payload; serializeJson(doc, payload);
                String topic = "homeassistant/" + nextDomain + "/" + Esp32::DEVICE_NAME + "_gpio" + String(gpio) + "/config";
                if (!publish(topic.c_str(), payload.c_str(), true)) pinOk = false;
                else ++count;
            }
            if (pinOk) { ioDomainKnown[gpio] = true; ioPublishedDomains[gpio] = nextDomain; }
            else ok = false;
        }
        JsonDocument configuration; deserializeJson(configuration, Esp32::getIOConfigurationJsonString());
        if (!sceneIDsLoaded) { deserializeJson(publishedScenes, Storage::readConfigFile("/io_ha_scenes.json")); sceneIDsLoaded = true; }
        JsonDocument next; JsonArray ids = next["ids"].to<JsonArray>();
        for (JsonVariantConst scene : configuration["scenes"].as<JsonArrayConst>()) ids.add(scene["id"]);
        for (const char* old : publishedScenes["ids"].as<JsonArrayConst>()) {
            bool retained = false; for (const char* id : ids) if (!strcmp(id, old)) retained = true;
            if (!retained) {
                if (!publish(String("homeassistant/button/" + Esp32::DEVICE_NAME + "_scene_" + old + "/config").c_str(), "", true)) ok = false;
                if (!publish(String("esp32/" + Esp32::DEVICE_NAME + "/io/scene/" + old + "/state").c_str(), "", true)) ok = false;
            }
        }
        String friendly = Esp32::getFriendlyName(), url = "http://" + Esp32::wifiManager.getIPString() + "/automation.html";
        unsigned int controls = 0; JsonDocument doc;
        auto sendControl = [&](const String& topic) { String payload; serializeJson(doc, payload); if (!publish(topic.c_str(), payload.c_str(), true)) ok = false; else ++controls; };
        IODiscovery::pause(doc, Esp32::DEVICE_NAME.c_str(), friendly.c_str(), url.c_str());
        sendControl("homeassistant/switch/" + Esp32::DEVICE_NAME + "_automation_pause/config");
        IODiscovery::scene(doc, Esp32::DEVICE_NAME.c_str(), friendly.c_str(), url.c_str(), JsonVariantConst());
        sendControl("homeassistant/button/" + Esp32::DEVICE_NAME + "_scene_all_off/config");
        for (JsonVariantConst scene : configuration["scenes"].as<JsonArrayConst>()) {
            IODiscovery::scene(doc, Esp32::DEVICE_NAME.c_str(), friendly.c_str(), url.c_str(), scene);
            sendControl("homeassistant/button/" + Esp32::DEVICE_NAME + "_scene_" + scene["id"].as<String>() + "/config");
        }
        String oldIDs, nextIDs; serializeJson(publishedScenes, oldIDs); serializeJson(next, nextIDs);
        if (ok && oldIDs != nextIDs) { if (Storage::writeConfigFile("/io_ha_scenes.json", nextIDs)) publishedScenes = next; else ok = false; }
        automationDiscoveryCount = controls;
        ioDiscoveryCount = count;
        ioDiscoveryReady = ok;
        if (ok) ioDiscoveryRevision = revision;
    }

    void sendJson(std::vector<String> names, std::vector<String> values, String topic, bool print2console) {
        const int docSize = MQTT_MAX_PACKET_SIZE;
        JsonDocument doc;
        for (size_t i = 0; i < names.size(); i++) {
            doc[names[i]] = values[i];
        }
        char json[docSize];
        size_t n = serializeJson(doc, json, docSize);
        if (n == 0) {
            Serial.println(F("Mqtt Error: JSON serialization failed (returned 0). Message not sent."));
        } else if (n >= docSize -1 ) {
            Serial.print(F("Mqtt Error: JSON message likely too large or truncated (n="));
            Serial.print(n); Serial.print(F(", docSize=")); Serial.print(docSize);
            Serial.println(F("). Message not sent."));
        } else {
            publish(topic.c_str(), json);
            if (print2console) {
                Serial.print(F("Mqtt: Sent JSON (")); Serial.print(n); Serial.print(F(" bytes): "));
                Serial.println(json);
            }
        }
    }

    void publishStandardMessage(
        const String& topic,
        const String& message_type,
        const JsonObjectConst& payload,
        const String& status,
        const String& command_id,
        const String& payload_type,
        bool printToConsole
    ) {
        if (!Mqtt::isEnabled) {
            if (printToConsole) {
                Serial.println(F("Mqtt: Publishing disabled, message not sent."));
            }
            return;
        }

        JsonDocument doc;

        doc["timestamp"] = Esp32::hourglass.getUtcIsoString();
        doc["sender"] = Esp32::DEVICE_NAME;
        doc["message_type"] = message_type;

        if (!status.isEmpty()) doc["status"] = status;
        if (!command_id.isEmpty()) doc["command_id"] = command_id;
        if (!payload_type.isEmpty()) doc["payload_type"] = payload_type;

        doc["payload"] = payload;

        const int buffer_size = MQTT_MAX_PACKET_SIZE;
        char json_buffer[buffer_size];
        size_t n = serializeJson(doc, json_buffer, buffer_size);

        if (n == 0) {
            Serial.println(F("Mqtt Error: JSON serialization failed for standard message."));
        } else if (n >= buffer_size - 1) {
            Serial.print(F("Mqtt Error: Standard JSON too large (n="));
            Serial.print(n); Serial.print(F(", buf=")); Serial.print(buffer_size);
            Serial.println(F(")."));
        } else {
            publish(topic.c_str(), json_buffer);
            if (printToConsole) {
                Serial.print(F("Mqtt: Sent (")); Serial.print(n); Serial.print(F("b) on '"));
                Serial.print(topic); Serial.print(F("': ")); Serial.println(json_buffer);
            }
        }
    }

    // MqttQueue method definitions
    void MqttQueue::add(String topic, String msg) {
        if (topics.size() >= MAX_MQTT_QUEUE_SIZE) {
            Serial.println(F("Mqtt Error: MQTT message queue full. Discarding new message."));
            Serial.print(F("Topic: ")); Serial.println(topic);
            return;
        }
        if(msg.length() > 3) {
            topics.push_back(topic);
            messages.push_back(msg);
        }
    }

    void MqttQueue::publish() {
        while (!topics.empty() && !messages.empty()) {
            if (!Mqtt::mqttClient.connected() ||
                !Mqtt::publish(topics.front().c_str(), messages.front().c_str())) {
                Serial.println(F("MQTT: Publish failed; keeping queued message for retry."));
                break;
            }
            Serial.print(F("MQTT: Sent queued message on "));
            Serial.println(topics.front());
            topics.pop_front();
            messages.pop_front();
        }
    }

} // namespace Mqtt
