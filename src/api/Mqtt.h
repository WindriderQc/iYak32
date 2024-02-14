#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <list>
#include <vector>
#include "Esp32.h"
//#include "Tools.h" 
//#include "alarmIO.h"

// define the number of bytes you want to access
#define EEPROM_SIZE 512 



/*
The maximum message size, including header, is 128 bytes by default. This is configurable via MQTT_MAX_PACKET_SIZE in PubSubClient.h or through build_flag in platformio.ini.
The keepalive interval is set to 15 seconds by default. This is configurable via MQTT_KEEPALIVE in PubSubClient.h.
client.loop() must be called...
*/

// TODO:  Seul l'IP est saved en EEPROM. Voir comment gerer le server_port aussi...

namespace Mqtt 
{
    IPAddress server_ip;
    int server_port = 1883;
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


    void incoming(char* topic, byte* message, unsigned int length) 
    {   
        //MqttMsg mqttMsg = sliceMqttMsg(topic, message, length);
        //Serial.print(F("New message arrived on topic: ")); Serial.println(mqttMsg.topicChar); 
        //Serial.print(F("Message: ")); Serial.println(mqttMsg.msgArray);

        String topicStr = String(topic);
        /*String msgStr = "";
        for (int i=0; i < length; i++) {
            msgStr += (char)message[i];
        }  */

        #ifdef VERBOSE
        Serial.println(topicStr);
        Serial.print("Msg bytes: "); Serial.println(length);
        //Serial.println(msgStr);
        #endif
       
        if(topicStr.indexOf(Esp32::DEVICE_NAME) >= 0) //  If the topic contains your ID 
        {    
            if(topicStr.indexOf("/config") >= 0)  //  If the topic contains ... 
            {
                        StaticJsonDocument<2048> config;
                        DeserializationError error = deserializeJson(config, message, length);  // Deserialize the JSON document  
                        if (error) { // Test if parsing succeeds.
                            Serial.print(F("deserializeJson() failed: "));  Serial.println(error.f_str()); return;
                        }

                        for (JsonObject elem : config.as<JsonArray>()) 
                        { 
                            unsigned int io = atoi(elem["io"]); 
                            const char* mode = elem["mode"];
                            const char* lbl = elem["lbl"]; 
                            int isA = atoi(elem["isA"]); 
                            //const char* pre = elem["pre"];    
                            //short tcpPort = config["port"] | 80;
                            #ifdef VERBOSE
                            Serial.println("Json deserialized");
                            Serial.println(io);
                            Serial.println(mode);
                            Serial.println(lbl);
                            Serial.println(isA);
                            // Serial.println(pre);
                            #endif
                            Esp32::configPin(io, mode, lbl, isA); 
                            //  --->   Als::pins[io].action_.pin_ = io;  

                                                /*
                            Serial.print("Setting alarm to pin......");
                                    Als::ActionInfo act; 
                                    act.pin_ = Esp32::ios[21]->config.gpio;
                                    act.type_ = Als::AlarmType::eREPEAT;
                                    act.callbackOn_  = []{Als::pins[21].handleAlarmOn();};
                                    act.callbackOff_ = []{Als::pins[21].handleAlarmOff();};    
                                    Als::pins[21].setup("LED1", act);
                            Serial.println("Done");*/
                                /* act.pin_ = Esp32::LED2_PIN;
                                    act.type_ = Als::AlarmType::eREPEAT;
                                    act.callbackOn_  = []{Als::pins[Esp32::LED2_PIN].handleAlarmOn();};
                                    act.callbackOff_ = []{Als::pins[Esp32::LED2_PIN].handleAlarmOff();};    
                                    Als::pins[Esp32::LED2_PIN].setup("LED2", act);
                                     */             
                        }
                        Serial.println(F("Config received and completed")); 
            }
            else if(topicStr.indexOf("/io/") >= 0)  //  If the topic contains ... 
            {
                        MqttMsg mqttMsg = sliceMqttMsg(topic, message, length);  //  TODO   plus de performance si on découpe sliceMqttMsg en petite fonction juste pour les token ou les array
                        
                        int io = atoi(mqttMsg.msgTokens[0]);   // then get 1st msg token should be the IO
                        
                        if(Esp32::validIO(io) == true) 
                        {
                            if(topicStr.indexOf("/on") >= 0) {
                                digitalWrite(io, HIGH); Serial.print(F("Setting ON output ")); Serial.println(io); return;
                            }
                            else if(topicStr.indexOf("/off") >= 0) {
                                digitalWrite(io, LOW);  Serial.print(F("Setting OFF output ")); Serial.println(io); return;
                            }
                            else if(topicStr.indexOf("/sunrise") >= 0) {
                                //  --->  Als::pins[io].setOnAlarm(atoi(mqttMsg.msgTokens[1]), atoi(mqttMsg.msgTokens[2]), atoi(mqttMsg.msgTokens[3])); return; //  IO:HH:MM:SS 
                            }
                            else if(topicStr.indexOf("/nightfall") >= 0) {
                               //  --->   Als::pins[io].setOffAlarm(atoi(mqttMsg.msgTokens[1]), atoi(mqttMsg.msgTokens[2]), atoi(mqttMsg.msgTokens[3])); return;  // IO:HH:MM:SS
                            }
                        } 
                        else Serial.println(F("Invalid IO"));

            }
            else if (topicStr.indexOf("/reboot") >= 0) //  If the topic contains ...
            {
                        Esp32::reboot();
            }

        }
        else {   /*Serial.println("Not my business!!!");*/      }
    }
  

    void subcription()
    {
        mqttClient.subscribe("esp32"); 
        mqttClient.subscribe(String("esp32/" + Esp32::DEVICE_NAME + "/#").c_str());
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


    void setup()
    {
        EEPROM.begin(EEPROM_SIZE);// initialize EEPROM with predefined size
        
        for(int i= 0; i<4; i++)  {
            server_ip[i] = EEPROM.read(i);
        }
        Serial.print(F("MQTT server IP address retrieved from EEPROM: "));  Serial.println(server_ip);


        mqttClient.disconnect();
        mqttClient.setServer(server_ip, server_port);
        //mqttClient.setBufferSize(512);  Semble pas marcher...  build flag dans platformio a la place
        mqttClient.setCallback(incoming);
      

        getCredentials() ? Serial.println("MQTT Credentials retreived") : Serial.println("ERROR - could not retreive MQTT credentials in mqtt.txt");

        int i = 0;
        while (!mqttClient.connected()) 
        {
            String clientId = "ESP32Client-" + String(random(0xffff), HEX); // Create a random client ID
             
            Serial.print(F("MQTT server...")); // Attempt to connect
            if (mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str())) {
                Serial.println(F("connected\n"));
                mqttClient.publish("esp32", String("hello from Esp32 " + Esp32::DEVICE_NAME).c_str() ); //Once connected, publish an announcement...
                subcription();// ... and resubscribe
            } 
            else  {
                Serial.print(F("connection failed, rc=")); Serial.print(mqttClient.state());  Serial.println(F(" try again in 5 seconds"));
                delay(5000);  
                i++;
                if(i > RECONNECT_TIMEOUT) Esp32::reboot();
            }
        }
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
        StaticJsonDocument<docSize> doc;

        for (int i = 0; i < names.size(); i++) {
            String st = String(names[i]);
            doc[st] = values[i];
        }

        char json[docSize];
        size_t n = serializeJson(doc, json, docSize);
       // mqttClient.publish(topic.c_str(), json, n);  json is a char[] and not uint8_t* so publish takes wrong prototype sees n as the retained flag and not the plength 
        mqttClient.publish(topic.c_str(), json);

        if(print2console) {
            String s = json;
            Serial.println(s);
        }
    }


} // namespace Mqtt


