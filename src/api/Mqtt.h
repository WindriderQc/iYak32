#pragma once

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <list>
#include <vector>
#include "SPIFFS.h"

//#include "Tools.h" 
//#include "alarmIO.h"

// define the number of bytes you want to access
#define EEPROM_SIZE 512 



/*
The maximum message size, including header, is 128 bytes by default. This is configurable via MQTT_MAX_PACKET_SIZE in PubSubClient.h or through build_flag in platformio.ini.
The keepalive interval is set to 15 seconds by default. This is configurable via MQTT_KEEPALIVE in PubSubClient.h.
client.loop() must be called...


on /esp32 channel, the device id is then used and commands structure follows....   ex:  eps32/ESP_35030/io/on

*/


namespace Mqtt 
{
    bool isEnabled = false;
 
    
    String serverIp;
    int port;
    String mqttUser = "";
    String mqttPass = "";

    const int MAX_TOKEN = 10;
    const int RECONNECT_TIMEOUT = 6; //  RECONNECT_TIMEOUT x 5 sec loop
    
    WiFiClient client;
    PubSubClient mqttClient(client);

    const int MAX_MQTT_QUEUE_SIZE = 20; // Max number of messages in the queue
 
    struct MqttMsg 
    {
        char topicChar[128]; 
        String topicStr;
        char *topicTokens[MAX_TOKEN];
        char msgArray[50];  
        String msgStr;
        char *msgTokens[MAX_TOKEN];   
    };


    struct MqttQueue 
    {
        std::list<String> topics;
        std::list<String> messages;

        void add(String topic, String msg)
        {
            if (topics.size() >= MAX_MQTT_QUEUE_SIZE) {
                Serial.println(F("Mqtt Error: MQTT message queue full. Discarding new message."));
                Serial.print(F("Topic: ")); Serial.println(topic); // Log discarded topic for debugging
                return; // Do not add the new message
            }

            // Existing message length check (can be reviewed separately)
            if(msg.length() > 3)
            {
                topics.push_back(topic);
                messages.push_back(msg); // Original TODO about buffer overflow is now addressed by queue limit
            }
        }
        
        void publish() 
        {
            while(topics.size() > 0)
            {
                  mqttClient.publish(topics.front().c_str(), messages.front().c_str() );
                  Serial.print(topics.front().c_str()); Serial.print(", "); Serial.println(messages.front().c_str() );
                  topics.pop_front();
                  messages.pop_front();
            }
        }

    } mqttQueue;

    MqttMsg sliceMqttMsg(char* topic, byte* message, unsigned int length) 
    {
        //  Slicing topic string into tokens and convert message from byte to array or string.  //  Allows access to each segment of the mqtt message   
        MqttMsg mqttMsg; // char topicChar[128];  //  create a copy because strtok modifies the original array
        // int i; // Original loop variable, no longer needed here for copying loops
       
        // Topic Processing
        size_t input_topic_len = strlen(topic);
        size_t max_topic_buf_len = sizeof(mqttMsg.topicChar) - 1;
        size_t chars_to_copy_topic = input_topic_len;

        if (input_topic_len >= sizeof(mqttMsg.topicChar)) {
            Serial.println(F("ERROR: MQTT topic too long, truncating."));
            chars_to_copy_topic = max_topic_buf_len;
        }
        strncpy(mqttMsg.topicChar, topic, chars_to_copy_topic);
        mqttMsg.topicChar[chars_to_copy_topic] = '\0'; // Ensure null termination

        mqttMsg.topicStr = ""; // Clear it first
        for (size_t k = 0; k < chars_to_copy_topic; ++k) {
            mqttMsg.topicStr += mqttMsg.topicChar[k];
        }

        // Message (Payload) Processing
        size_t max_msg_buf_len = sizeof(mqttMsg.msgArray) - 1;
        size_t chars_to_copy_msg = length;

        if (length >= sizeof(mqttMsg.msgArray)) {
            Serial.println(F("ERROR: MQTT message payload too long, truncating."));
            chars_to_copy_msg = max_msg_buf_len;
        }

        mqttMsg.msgStr = ""; // Clear it first
        for (unsigned int k = 0; k < chars_to_copy_msg; ++k) {
            mqttMsg.msgArray[k] = (char)message[k];
            mqttMsg.msgStr += (char)message[k]; // Build String version from (truncated) payload
        }
        mqttMsg.msgArray[chars_to_copy_msg] = '\0'; // Ensure null termination

        char *ptr = NULL;
        byte index = 0;
        ptr = strtok(mqttMsg.topicChar, "/");  // takes a list of delimiters
        while(ptr != NULL) {
            mqttMsg.topicTokens[index] = ptr;  index++;  ptr = strtok(NULL, "/");  // takes a list of delimiters
        }
       
        index = 0;
        ptr = NULL;
        ptr = strtok(mqttMsg.msgArray, ":");  // takes a list of delimiters
        while(ptr != NULL) {
            mqttMsg.msgTokens[index] = ptr;  index++;  ptr = strtok(NULL, ":");  // takes a list of delimiters
        }
        
        return mqttMsg;
    }


    void subcription(String deviceName)
    {
        mqttClient.subscribe("esp32"); 
        mqttClient.subscribe(String("esp32/" + deviceName + "/#").c_str());
    }

    bool getCredentials(bool isSpiffsMounted) // Added isSpiffsMounted parameter
    {
        if (!isSpiffsMounted) { // Use parameter
            Serial.println(F("Mqtt Error: SPIFFS not mounted. Cannot load MQTT credentials from mqtt.txt."));
            return false;
        }

        File configFile = SPIFFS.open("/mqtt.txt", "r"); // Open the config file for reading

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
            return true;
        }

        return false;
    }


    bool setup( String deviceName, String mqttIP, bool isSpiffsMounted, int server_port_param = 1883) // Added isSpiffsMounted
    {
        serverIp = mqttIP;
        port = server_port_param;
        Serial.print(F("MQTT server IP address retrieved: "));  Serial.println(serverIp);
        
        mqttClient.disconnect();

        IPAddress default_ip(0, 0, 0, 0);

        if(serverIp == "")  mqttClient.setServer("specialblend.ca", port);
        else                 mqttClient.setServer(serverIp.c_str(), port);
       
        //mqttClient.setBufferSize(512);  Semble pas marcher...  build flag dans platformio a la place
        //mqttClient.setCallback(incomingCallback);
      

        getCredentials(isSpiffsMounted) ? Serial.println("MQTT Credentials retreived") : Serial.println("ERROR - could not retreive MQTT credentials in mqtt.txt");

        int i = 0;
        while (!mqttClient.connected()) 
        {
            String clientId = "ESP32Client-" + String(random(0xffff), HEX); // Create a random client ID
             
            Serial.print(F("MQTT server...")); // Attempt to connect
            if (mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
                Serial.println(F("connected"));
                mqttClient.publish("esp32", String("hello from Esp32 " + deviceName).c_str() ); //Once connected, publish an announcement...
                subcription(deviceName);// ... and resubscribe
               
            } 
            else  {
                Serial.print(F("connection failed, rc=")); Serial.print(mqttClient.state());  Serial.println(F(" try again in 5 seconds"));
                delay(5000);  
                i++;
              
                if(i > RECONNECT_TIMEOUT)  return false; 
            }
        }
         
        return true;
    }


   /* void checkConnection()
    {
        if(mqttClient.connected()) {  //  returns 0 when connected!
            //Serial.println("MQTT server disconnected!!!  Reconnecting...."); 
            //Mqtt::setup();   // if client was disconnected then try to reconnect again 
        } 

    }*/


    void loop()
    {      
       // checkConnection();  
        mqttClient.loop();   
        mqttQueue.publish();      //Mqtt::mqttQueue.add("esp32/sensors", btnBlue.loop());
    }


    void sendJson(std::vector<String> names, std::vector<String> values, String topic, bool print2console = false)   
    {
        const int docSize = MQTT_MAX_PACKET_SIZE;
        JsonDocument doc;

        for (int i = 0; i < names.size(); i++) {
            String st = String(names[i]);
            doc[st] = values[i];
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
            // Serialization successful and fits well within the buffer
            mqttClient.publish(topic.c_str(), json, n); // Publish with explicit length 'n'

            if (print2console) {
                Serial.print(F("Mqtt: Sent JSON (")); Serial.print(n); Serial.print(F(" bytes): "));
                Serial.println(json);
            }
        }
    }


} // namespace Mqtt


