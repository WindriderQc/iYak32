#include "Mqtt.h"
#include "api/Esp32.h"    // For Esp32::hourglass and Esp32::DEVICE_NAME
#include "api/SystemLog.h" // For error logging
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

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
        if (!isEnabled) return;
        Serial.println(F("MQTT: Force reconnect requested."));
        SystemLog::logError("MQTT: Force reconnect requested", 0);
        connectAttemptCount_ = 0;
        backoffMultiplier_ = 0;
        backoffDuration_ = BACKOFF_BASE_MS;
        lastConnectAttempt_ = 0;
        currentState_ = MqttState::CONNECTING;
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

        String clientId = "ESP32Client-" + String(random(0xffff), HEX);
        Serial.printf("MQTT: Attempt %d/%d... ", connectAttemptCount_, MAX_ATTEMPTS_PER_BURST);

        if (mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
            Serial.println(F("connected"));
            mqttClient.publish("esp32", String("hello from " + pendingDeviceName_).c_str());
            subscription(pendingDeviceName_);
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
            mqttClient.publish(topic.c_str(), json, n);
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

        doc["timestamp"] = Esp32::hourglass.getDateTimeString(false, true);
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
            Mqtt::mqttClient.publish(topic.c_str(), json_buffer, n);
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
        while(topics.size() > 0) {
            Mqtt::mqttClient.publish(topics.front().c_str(), messages.front().c_str() );
            Serial.print(topics.front().c_str()); Serial.print(", "); Serial.println(messages.front().c_str() );
            topics.pop_front();
            messages.pop_front();
        }
    }

} // namespace Mqtt
