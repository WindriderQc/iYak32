#include "Mqtt.h"
#include "api/Esp32.h"    // For Esp32::hourglass and Esp32::DEVICE_NAME
#include <Arduino.h> // For Serial, String, random, delay, etc.
#include <WiFi.h>      // For WiFiClient
#include <PubSubClient.h> // For PubSubClient
#include <ArduinoJson.h>  // For JsonDocument if used in method bodies (sendJson)
#include <SPIFFS.h>     // For File, SPIFFS (used in getCredentials)
// Esp32.h might be needed if Esp32::spiffsMounted was used directly, but it's now passed as a parameter.

// Define Namespace Variables
namespace Mqtt {
    bool isEnabled = false;
    String serverIp = "";
    int port = 1883;
    String mqttUser = "";
    String mqttPass = "";
    WiFiClient client; // Definition
    PubSubClient mqttClient(client); // Definition and initialization with Mqtt::client
    MqttQueue mqttQueue; // Definition

    // Function Definitions
    MqttMsg sliceMqttMsg(char* topic, byte* message, unsigned int length) {
        MqttMsg mqttMsg;
        size_t input_topic_len = strlen(topic);
        size_t max_topic_buf_len = sizeof(mqttMsg.topicChar) - 1;
        size_t chars_to_copy_topic = input_topic_len;

        if (input_topic_len >= sizeof(mqttMsg.topicChar)) {
            Serial.println(F("ERROR: MQTT topic too long, truncating."));
            chars_to_copy_topic = max_topic_buf_len;
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
        }

        mqttMsg.msgStr = "";
        for (unsigned int k = 0; k < chars_to_copy_msg; ++k) {
            mqttMsg.msgArray[k] = (char)message[k];
            mqttMsg.msgStr += (char)message[k];
        }
        mqttMsg.msgArray[chars_to_copy_msg] = '\0';

        char *ptr = NULL;
        byte index = 0;
        ptr = strtok(mqttMsg.topicChar, "/");
        while(ptr != NULL) {
            mqttMsg.topicTokens[index] = ptr;  index++;  ptr = strtok(NULL, "/");
        }

        index = 0;
        ptr = NULL;
        ptr = strtok(mqttMsg.msgArray, ":");
        while(ptr != NULL) {
            mqttMsg.msgTokens[index] = ptr;  index++;  ptr = strtok(NULL, ":");
        }
        return mqttMsg;
    }

    void subscription(String deviceName) { // Corrected spelling
        mqttClient.subscribe("esp32");
        mqttClient.subscribe(String("esp32/" + deviceName + "/#").c_str());
    }

    bool getCredentials(bool isSpiffsMounted) {
        if (!isSpiffsMounted) {
            Serial.println(F("Mqtt Error: SPIFFS not mounted. Cannot load MQTT credentials from mqtt.txt."));
            return false;
        }
        File configFile = SPIFFS.open("/mqtt.txt", "r");
        if (!configFile) {
            Serial.println("Failed to open mqtt file");
            return false;
        }
        Serial.println("mqtt.txt found for user:password");
        if(configFile.available()) {
            String line = configFile.readStringUntil('\n');
            line.trim();
            int separatorIndex = line.indexOf(':');
            if (separatorIndex != -1) {
                mqttUser = line.substring(0, separatorIndex);
                mqttPass = line.substring(separatorIndex + 1);
            }
            configFile.close(); // Close file
            return true;
        }
        configFile.close(); // Close file
        return false;
    }

    bool setup( String deviceName, String mqttIP, bool isSpiffsMounted, int server_port_param) {
        serverIp = mqttIP;
        port = server_port_param;
        Serial.print(F("MQTT server IP address retrieved: "));  Serial.println(serverIp);

        mqttClient.disconnect();
        // IPAddress default_ip(0, 0, 0, 0); // Not used
        if(serverIp == "")  mqttClient.setServer("specialblend.ca", port);
        else                 mqttClient.setServer(serverIp.c_str(), port);

        getCredentials(isSpiffsMounted) ? Serial.println("MQTT Credentials retreived") : Serial.println("ERROR - could not retreive MQTT credentials in mqtt.txt");

        int i = 0;
        while (!mqttClient.connected()) {
            String clientId = "ESP32Client-" + String(random(0xffff), HEX);
            Serial.print(F("MQTT server..."));
            if (mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
                Serial.println(F("connected"));
                mqttClient.publish("esp32", String("hello from Esp32 " + deviceName).c_str() );
                subscription(deviceName); // Corrected spelling
            } else {
                Serial.print(F("connection failed, rc=")); Serial.print(mqttClient.state());  Serial.println(F(" try again in 5 seconds"));
                delay(5000);
                i++;
                if(i > RECONNECT_TIMEOUT)  return false;
            }
        }
        return true;
    }

    void loop() {
        mqttClient.loop();
        mqttQueue.publish();
    }

    void sendJson(std::vector<String> names, std::vector<String> values, String topic, bool print2console) {
        const int docSize = MQTT_MAX_PACKET_SIZE; // This constant is fine here as it's local to function
        JsonDocument doc;
        for (size_t i = 0; i < names.size(); i++) { // Use size_t for vector iteration
            doc[names[i]] = values[i];
        }
        char json[docSize];
        size_t n = serializeJson(doc, json, docSize);
        if (n == 0) {
            Serial.println(F("Mqtt Error: JSON serialization failed (returned 0). Message not sent."));
            if (print2console) {
                Serial.println(F("Mqtt: (No message sent due to serialization error)"));
            }
        } else if (n >= docSize -1 ) {
            Serial.print(F("Mqtt Error: JSON message likely too large for buffer or truncated (written bytes n="));
            Serial.print(n);
            Serial.print(F(", buffer size docSize="));
            Serial.print(docSize);
            Serial.println(F("). Message not sent."));
            if (print2console) {
                Serial.print(F("Mqtt: Truncated/Problematic JSON (not sent): "));
                Serial.println(json);
            }
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
        doc["sender_id"] = Esp32::DEVICE_NAME;
        doc["message_type"] = message_type;

        if (!status.isEmpty()) {
            doc["status"] = status;
        }
        if (!command_id.isEmpty()) {
            doc["command_id"] = command_id;
        }
        if (!payload_type.isEmpty()) {
            doc["payload_type"] = payload_type;
        }

        doc["payload"] = payload;

        const int buffer_size = MQTT_MAX_PACKET_SIZE;
        char json_buffer[buffer_size];
        size_t n = serializeJson(doc, json_buffer, buffer_size);

        if (n == 0) {
            Serial.println(F("Mqtt Error: JSON serialization failed for standard message (returned 0). Message not sent."));
            if (printToConsole) {
                Serial.println(F("Mqtt: (No message sent due to serialization error)"));
            }
        } else if (n >= buffer_size - 1) {
            Serial.print(F("Mqtt Error: Standard JSON message likely too large or truncated (n="));
            Serial.print(n); Serial.print(F(", buffer_size=")); Serial.print(buffer_size);
            Serial.println(F("). Message not sent."));
            if (printToConsole) {
                Serial.print(F("Mqtt: Truncated/Problematic JSON (not sent): "));
                Serial.println(json_buffer);
            }
        } else {
            Mqtt::mqttClient.publish(topic.c_str(), json_buffer, n);
            if (printToConsole) {
                Serial.print(F("Mqtt: Sent Standard JSON (")); Serial.print(n); Serial.print(F(" bytes) on topic '"));
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
            // Use Mqtt::mqttClient directly as these methods are part of Mqtt namespace
            Mqtt::mqttClient.publish(topics.front().c_str(), messages.front().c_str() );
            Serial.print(topics.front().c_str()); Serial.print(", "); Serial.println(messages.front().c_str() );
            topics.pop_front();
            messages.pop_front();
        }
    }

} // namespace Mqtt
