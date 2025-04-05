#pragma once

#include <Arduino.h>
#include <esp_log.h>

#include <ArduinoOTA.h>
#include <ArduinoJson.h>   

#include <Wire.h>    // TODO Dépends de si on utilise i2C...  ne devrait ptete pas etre aussi générique

#include "IPin.h"
#include "Storage.h"

#include "WifiManager.h"
#include "Hourglass.h"
#include "Mqtt.h"
#include "devices/Buzzer.h"




/* 

    ESP32 API / Librairie providing all methods to configure ESP32 IOs, WIFI, Mqtt, time and various devices that can be added/configure to the ESP.
    Needs a mqtt.txt file in data folder containing user:pass
    
    Mqtt:
    on /esp32 channel, the device id is then used and commands structure follows....   ex:  eps32/ESP_35030/io/on
    
    
    TODO: all msg should be json { status: , msg: , data:  , ... }   sill to uniformize


    Esp32 are listening to :
        - esp32
        - esp32/DEVICE_NAME

        esp32/DEVICE_NAME/io/on msg: GPIO
        esp32/DEVICE_NAME/io/off msg: GPIO
        esp32/DEVICE_NAME/io/sunrise msg: GPIO:HH:MM:SS
        esp32/DEVICE_NAME/io/nightfall msg: GPIO:HH:MM:SS
        esp32/DEVICE_NAME/reboot
        esp32/DEVICE_NAME/config msg: Stringify->config = [ { io: 2, mode: "IN", lbl: "A0", isA: 0, pre: "none" },{ io: 4, mode: "IN", lbl: "A1", isA: 0, pre: "none" }]
*/



namespace Esp32   //  ESP 32 configuration and helping methods
{
   
    const String DEVICE_NAME = String("ESP_") + String((uint16_t)(ESP.getEfuseMac()>>32));

    const String CONFIG_FILENAME =  "/esp32config.json";
    const int CONFIG_FILE_MAX_SIZE = 1024;   // TODO: really required? Since JSON is dynamic...
    JsonDocument configJson_;
    String configString_;
   
    bool spiffsMounted = false;


    const int ADC_Max = 4095;    

    #define BATTERY_READ_PIN 35 // Pin used to read battery voltag   (Huzzah32 - A13 - pin 35)   //   TODO :  devrait plus etre lié a la config / profile 
    String batteryText;         // String variable to hold text for battery voltage
    float vBAT = 0;             // Float variable to hold battery voltage
    byte vBATSampleSize = 5;    // How many time we sample the battery

    // Pin qty per model definitions
    #define HUZZAH32  39
    #define WEMOSIO   20  // tbd..    
    Pin* ios[HUZZAH32];  //  39 pins for esp32   //  TODO: could be #defines NBRPIN selon le model WEMOSIO, EPS, etc.


    WifiManager wifiManager;
    Hourglass hourglass;
    Buzzer buzzer;  //  TODO:  devrait etre un device configurable...  pas dans le namespace ESP32
    





    void setVerboseLog()    { esp_log_level_set("*", ESP_LOG_DEBUG);    }  //  require #include <esp_log.h>
  
    float getCPUTemp()      { return temperatureRead();                 }
    
    int getCPUFreq()        { return ESP.getCpuFreqMHz();               }
    
    int getRemainingHeap()  { return ESP.getFreeHeap();                 }

    bool validIO(int io) 
    {
        Serial.print("valid io: ");
        Serial.println(io);
        if(ios[io] != nullptr) {
            if(ios[io]->config.gpio != 0) {
                 return true;
            }
        }

        return false;
    }
   
    bool validIO(String io) { return validIO(io.toInt());               }
    
    void ioSwitch(int pin)  { digitalWrite(pin, !digitalRead(pin));     }
   
    void ioBlink(int pin, int timeon, int timeoff, int iteration)
    {
        for (int i = 0; i < iteration; i++)
        {
            ioSwitch(pin);
            ///delay(timeon);  
            delay(timeon);  
            ioSwitch(pin);
            ///delay(timeoff);  
            delay(timeoff);
        }
    }

    void configPin(int gpio ,  const char* pinMode,  const char* label = "", bool isAnalog = false)/* Accept  "INPULL", "INPULLD", "IN" and "OUT" as pinMode.   */
    {
       // if(ios[gpio] != nullptr) delete(ios[gpio]); 
        ios[gpio] = new Pin(pinMode,gpio, label, isAnalog);
    }

    void reboot() 
    {  
        Serial.println(F("!!!  Rebooting device..."));
        delay(1500);
        ESP.restart();                           
    }

    int i2cScanner()
    {
        byte count = 0;
        Serial.print (" -- I2C Scanning -- \n");
        Wire.begin();
        for (byte i = 8; i < 120; i++)
        {
        Wire.beginTransmission (i);
        if (Wire.endTransmission () == 0)
            {
            Serial.print ("Found address: ");
            Serial.print (i, DEC);
            Serial.print (" (0x");
            Serial.print (i, HEX);
            Serial.println (")");
            count++;
            } // end of good response
        } // end of for loop
        Serial.println ("Scanning Done.");
        Serial.print ("Found ");    Serial.print (count, DEC);    Serial.println (" device(s).");

        return count;
    } 
    
    // Check the battery voltage     vBAT = between 0 and 4.2 expressed as volts
    float getBatteryVoltage()
    {
        vBAT = (127.0f / 100.0f) * 3.30f * float(analogRead(BATTERY_READ_PIN)) / 4095.0f; // Calculates the voltage left in the battery
        return vBAT;                                                                       
    }
    //  Convert batt voltage to %
    float getBattRemaining(bool print = false) 
    {
        for (byte i = 0; i < vBATSampleSize; i++) // Average samples together to minimize false readings
        {
            vBAT += ceilf(getBatteryVoltage() * 100) / 100; // Work out battery voltage from DAC and round to 2 decimal places
        }

        vBAT /= vBATSampleSize;

        if(print) {
            batteryText = String(vBAT);
            Serial.print("Battery Voltage: "); 
            Serial.print(batteryText);  
            Serial.println("V");  
        }
        
        return vBAT;
    }


    // Helper function to convert a json to a string
    String getJsonString(JsonDocument doc, bool isPretty = false) 
    {
        String str = "";
        isPretty ? serializeJsonPretty(doc, str) : serializeJson(doc, str);
        return str;
    }



    void mqttIncoming(char* topic, byte* message, unsigned int length) {
        
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
                        JsonDocument config;
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
                        Mqtt::MqttMsg mqttMsg = Mqtt::sliceMqttMsg(topic, message, length);  //  TODO   plus de performance si on découpe sliceMqttMsg en petite fonction juste pour les token ou les array
                        
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







    
    void executeConfig()
    {
        Serial.println("\nExecuting Config!");

        wifiManager.setSSID(configJson_["ssid"]);
        wifiManager.setPASS(configJson_["pass"]);

        wifiManager.setup(true, configJson_["ssid"],configJson_["pass"] );  //  Set WIFI connection and OTA. Access point if cannot reach SSID

        //  if not on access point, synchronize system time
        if(Esp32::wifiManager.isConnected()) {
            if(Esp32::hourglass.setupTimeSync()) Esp32::hourglass.getDateTimeString(true);
        }


        Mqtt::isEnabled =           configJson_["isMqtt"];     
        Mqtt::isConfigFromServer =  configJson_["isConfigFromServer"]; 
        Mqtt::port_ =               configJson_["mqttport"];

        IPAddress mqttIP = IPAddress(configJson_["ip0"], configJson_["ip1"], configJson_["ip2"], configJson_["ip3"]);
        
        if(Mqtt::isEnabled) {
            Mqtt::mqttClient.setCallback(mqttIncoming);

            if (!Mqtt::setup(DEVICE_NAME, mqttIP, Mqtt::port_ ))     Serial.print("Mqtt setup fail"); 
            else                                                     Serial.print("Mqtt setup completed");                    
                                                                                                                
        } 
    }


    bool loadConfig(JsonDocument* returnDoc = nullptr, bool doExecuteConfig = false)
    {   
        String name = Esp32::CONFIG_FILENAME;
         if (!SPIFFS.exists(name)) {
            Serial.print("esp32Config file "); Serial.print(name); Serial.println(" not found; using system defaults.");
            return false;
         } else {
            // read file into a string
            Serial.print("Loading preferences from file ");
            Serial.println(Esp32::CONFIG_FILENAME);
            String file_content = Storage::readFile(Esp32::CONFIG_FILENAME);
            int config_file_size = file_content.length();
            Serial.println("Config file size: " + String(config_file_size));

            if(config_file_size > CONFIG_FILE_MAX_SIZE) {   // corrupted SPIFFS files can return data beyond their declared size.
                Serial.println("Config file too large, maybe corrupt, removing");
                Storage::removeConfigFile(Esp32::CONFIG_FILENAME);
                return false;
            }

            JsonDocument doc;
            auto error = deserializeJson(doc, file_content);
            if ( error ) { 
                Serial.println("Error interpreting config file");
                return false;
            }

            Storage::dumpFile(Esp32::CONFIG_FILENAME);

            if (returnDoc)  *returnDoc = doc; // if a return doc is provded, Copy the content of doc into returnDoc

            configJson_ = doc; 
            configString_ = getJsonString(configJson_);

            if(doExecuteConfig)  executeConfig(); 

            return true;
        } 
    }


    bool saveConfig(JsonDocument config, bool reboot = false)
    {
        SPIFFS.exists(Esp32::CONFIG_FILENAME)  ?  Serial.print("Updating ") :  Serial.print("Creating "); 
        Serial.println(Esp32::CONFIG_FILENAME);

        configJson_ = config;
        configString_ = getJsonString(configJson_);

        Storage::writeFile(Esp32::CONFIG_FILENAME, configString_);

        
        Storage::dumpFile(Esp32::CONFIG_FILENAME);

        if(reboot) Esp32::reboot();
        else loadConfig();  //  Actalize local variable without executing the new config

        return true;
    }




    //////////////////////////////////////////////
    // Main
    //////////////////////////////////////////////

    void setup() 
    {
       
        Serial.println("\nStarting internal SPIFFS filesystem");

        if(!SPIFFS.begin(false)) {
            Serial.println("SPIFFS Mount failed\nDid not find filesystem - this can happen on first-run initialisation; starting format"); 
            ioBlink(LED_BUILTIN,100, 100, 4); // Show SPIFFS failure
            // format if begin fails
            if (!SPIFFS.begin(true)) {
                Serial.println("SPIFFS mount failed\nFormatting not possible - check if a SPIFFS partition is present for your board?");
                ioBlink(LED_BUILTIN,100, 100, 8); // Show SPIFFS failure
                spiffsMounted = false;
            } else {
                Serial.println("Formatting");
                spiffsMounted = false;
            }
        } else {
            Serial.println("setup -> SPIFFS mounted successfully");
            spiffsMounted = true;
            Storage::listDir("/", 4);  //  TODO : show only files on root, not folders....  

            // loading config json and saving it as JsonDocument. If loading fails, create a default json and save the default file.
            if(loadConfig(&configJson_) == false) {   
                 // Create a dictionary (key-value pairs) to store configurations and save/load from config file
                JsonDocument configDoc;
                 // default esp32config.jsom if not found on SPIFFS
                configDoc["isMqtt"] = false;
                configDoc["isConfigFromServer"] = false;
                configDoc["ssid"] = "";
                configDoc["pass"] ="";
                configDoc["mqttport"] = 1883;
                configDoc["ip0"] = "";
                configDoc["ip1"] = "";
                configDoc["ip2"] = "";
                configDoc["ip3"] = "";
                configDoc["profileName"] = "default_ESP32";
                Serial.println("setup -> Could not read Config file -> initializing new file");
               
                if (saveConfig(configDoc)) Serial.println("setup -> Config file saved");   
            
            }
            else { 
                Serial.println("ConfigJson was retreved and configuration executed...");              
            }
            
            executeConfig();  
        }




        i2cScanner();


    }


    void loop() 
    {
        //timeSinceBoot += millis();     TODO: timeSinceboot devrait etre extern
        wifiManager.loop();   //  reconnect if connection is lost and handle OTA
       //TODO  devrait y avoir un readIO ici, non??
        buzzer.loop();     

        if(Mqtt::isEnabled) Mqtt::loop();   // listen for incomming subscribed topic, process receivedCallback, and manages msg publish queue   
    }


    
    


    namespace GPS {
        const int lon = 77;
        const int lat = 55;
    }

} // namespace Esp32




