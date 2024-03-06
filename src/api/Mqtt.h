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
*/


namespace Mqtt 
{
    bool isEnabled = false;
    bool isConfigFromServer = false;
    
    IPAddress server_ip;
    String mqttUser = "";
    String mqttPass = "";

    const int MAX_TOKEN = 10;
    const int RECONNECT_TIMEOUT = 6; //  RECONNECT_TIMEOUT x 5 sec loop
    
    WiFiClient client;
    PubSubClient mqttClient(client);

 
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
            if(msg.length() > 3)
            {
                topics.push_back(topic);
                messages.push_back(msg);//  TODO    Buffer overflow??  Top ressources?
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
        int i;   
       
        for (i = 0; i < (strlen(topic)); i++) {
            mqttMsg.topicChar[i] = topic[i];
            mqttMsg.topicStr += topic[i];
        }    
        mqttMsg.topicChar[i] = '\0'; 

        for (i = 0; i < length; i++) {
            mqttMsg.msgArray[i] = (char)message[i];
            mqttMsg.msgStr += (char)message[i];
        }  
        mqttMsg.msgArray[i] = '\0';      

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

    bool getCredentials()
    {
        
        if (!SPIFFS.begin(true)) {   //  TODO:  probleme si on mount le SPIFF 2 fois??
            Serial.println("Failed to mount SPIFFS");
            return false;
        }

        File configFile = SPIFFS.open("/mqtt.txt", "r"); // Open the config file for reading

        if (!configFile) {
            Serial.println("Failed to open mqtt file");
            return false;
        }

        Serial.println("mqtt.txt found for user and password");

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


    bool setup( String deviceName, int server_port = 1883)
    {

        Serial.print(F("MQTT server IP address retrieved: "));  Serial.println(server_ip);

        mqttClient.disconnect();
        mqttClient.setServer(server_ip, server_port);
        //mqttClient.setBufferSize(512);  Semble pas marcher...  build flag dans platformio a la place
        //mqttClient.setCallback(incomingCallback);
      

        getCredentials() ? Serial.println("MQTT Credentials retreived") : Serial.println("ERROR - could not retreive MQTT credentials in mqtt.txt");

        int i = 0;
        while (!mqttClient.connected()) 
        {
            String clientId = "ESP32Client-" + String(random(0xffff), HEX); // Create a random client ID
             
            Serial.print(F("MQTT server...")); // Attempt to connect
            if (mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
                Serial.println(F("connected\n"));
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


    void checkConnection()
    {
        if(mqttClient.connected()) {  //  returns 0 when connected!
            //Serial.println("MQTT server disconnected!!!  Reconnecting...."); 
            //Mqtt::setup();   // if client was disconnected then try to reconnect again 
        } 

    }


    void loop()
    {      
        checkConnection();  
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
        size_t n = serializeJson(doc, json, docSize);   //  TODO:  valider comment gerer si le json est plus gros que MAX MQTT
        mqttClient.publish(topic.c_str(), json);

        if(print2console) {
            String s = json;
            Serial.println(s);
        }
    }


} // namespace Mqtt


