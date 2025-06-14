#pragma once
/* 
Add to platformio.ini:
    lib_deps =
        https://github.com/me-no-dev/ESPAsyncWebServer.git  ; required for www.h
*/
//#include <vector>
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <SPIFFS.h>      // Added for SPIFFS object usage
#include "api/Mqtt.h"    // Added for Mqtt namespace usage
#include "api/Esp32.h"
#include "api/JsonTools.h" // Added for JsonTools::getJsonString


#ifdef HOCKEY_MODE // Assuming HOCKEY_MODE is defined in main.cpp or a config
    #include "Hockey.h"
#endif

#ifdef BASIC_MODE // Assuming BASIC_MODE is defined in main.cpp or a config
    #include "BasicMode.h"
#endif



namespace www
{

    HTTPClient http;  

    const uint serverPort = 80;
    
    AsyncWebServer server(serverPort);
   
    
    void sendProcessedHtml(AsyncWebServerRequest *request, const char *filePath, String (*processor)(const String&)) {
        File file = SPIFFS.open(filePath, "r");
        if (!file) {
            request->send(500, "text/plain", "Failed to open file");
            return;
        }
    
        // Create a string to hold the processed HTML
        String htmlContent;
    
        // Read the file line by line and process placeholders
        while (file.available()) {
            String line = file.readStringUntil('\n');
            htmlContent += processor(line); // Use the provided processor function
        }
        file.close(); // Close the file after reading
    
        Serial.print("Free heap after reading file: ");
        Serial.println(ESP.getFreeHeap());
    
        // Send the processed HTML with the correct Content-Type
        request->send(200, "text/html", htmlContent);
    }



    String defaultProcessor(const String& var) {
        String processedVar = var; // Create a copy of the input string, where all replacements will be made
    
        if (processedVar.indexOf("%TIME%") >= 0) {
            processedVar.replace("%TIME%", Esp32::hourglass.getDateTimeString());
        }
        if (processedVar.indexOf("%STATE%") >= 0) {
            processedVar.replace("%STATE%", digitalRead(BUILTIN_LED) ? "ON" : "OFF");
        }
      
 
        if (processedVar.indexOf("%LON%") >= 0) {
            processedVar.replace("%LON%", String(Esp32::GPS::lon));
        }
        if (processedVar.indexOf("%LAT%") >= 0) {
            processedVar.replace("%LAT%", String(Esp32::GPS::lat));
        }
        if (processedVar.indexOf("%TEMP%") >= 0) {
            processedVar.replace("%TEMP%", String(Esp32::getCPUTemp()));
        }
        return processedVar; // Return the modified string
    }

    String setupProcessor(const String& var) {
        String processedVar = var; 

        if (processedVar.indexOf("%TIME%") >= 0) {
            processedVar.replace("%TIME%", Esp32::hourglass.getDateTimeString());
        }
        if (processedVar.indexOf("%LOCALIP%") >= 0) {
            processedVar.replace("%LOCALIP%", Esp32::wifiManager.getIPString());
        }
        if (processedVar.indexOf("%SSID%") >= 0) {
            processedVar.replace("%SSID%", String(Esp32::wifiManager.getSSID()));
        }
        if (processedVar.indexOf("%MQTTURL%") >= 0) {
            processedVar.replace("%MQTTURL%", Mqtt::serverIp);
        }
        if (processedVar.indexOf("%CONFIG%") >= 0) {
            JsonDocument d = Esp32::configJson_;
            d["pass"] = ""; // Suppress password for security
            processedVar.replace("%CONFIG%", JsonTools::getJsonString(d)); // Changed to JsonTools
        }
     
        return processedVar; 
    }

    
   String processor(const String& var)   //  this will be called for every %PLACEHOLDER% in the served html file
    {

        if(var == "STATE")    {  return digitalRead(BUILTIN_LED) ? "ON" : "OFF";    }

        if(var == "TIME")     {  return Esp32::hourglass.getDateTimeString();       }
        if(var == "TEMP")     {  return String(Esp32::getCPUTemp());                }

        if(var == "LON")      {  return String(Esp32::GPS::lon);                    }
        if(var == "LAT")      {  return String(Esp32::GPS::lat);                    }

        if(var == "LOCALIP")  {  return Esp32::wifiManager.getIPString();           }
        if(var == "SSID")     {  return String(Esp32::wifiManager.getSSID());       }


        if(var == "MQTTURL")   {  return Mqtt::serverIp;                            }
       
        if(var == "CONFIG")   {  
            JsonDocument d = Esp32::configJson_;
            d["pass"] = "";   //  suppress password for security.  this forces user to enter it back everytime.
            return JsonTools::getJsonString(d); // Changed to JsonTools
            
        }

        return String();
    }

#ifdef HOCKEY_MODE 
    unsigned long startTime;
    unsigned long pausedTime = 0;
    bool isPaused = true;
          
    void handlePause() {
        if (!isPaused) {
          pausedTime = millis() - startTime;
          isPaused = true;
        } else {
          startTime = millis() - pausedTime;
          isPaused = false;
        }
        hockey.pause();
      }
      
      void handleReset() {
        startTime = millis();
        pausedTime = 0;
        isPaused = true;
        hockey.reset();
      }
#endif


    
    void setup() 
    {
        Serial.print(F("\nWWW from SPIFFS setup... ")); 

       // Storage::dumpFile("/esp32.css");
       /* File file = SPIFFS.open("/esp32.css", "r");
        if (file) {
            while (file.available()) {
                Serial.write(file.read());
            }
            file.close();
        } else {
            Serial.println("Failed to open sbqc.css");
        }*/

       /* server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
            sendProcessedHtml(request, "/index.html", defaultProcessor);
        });*/
    
        /*server.on("/setup.html", HTTP_GET, [](AsyncWebServerRequest *request) {
            sendProcessedHtml(request, "/setup.html", setupProcessor);
        });*/

       
        server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
                JsonDocument cnf;
                String cnfStr = "";
                Serial.println("Loading config from json: ");
                if(Esp32::loadConfig(false, &cnf)) {  
                    cnf["pass"] = ""; // Blank out the password for security
                    cnfStr = JsonTools::getJsonString(cnf, true); // Changed to JsonTools
                    Serial.println(Esp32::configString_);
                    request->send(200, "application/json", cnfStr); // Send the JSON response with the appropriate content type
                } else {
                    request->send(500, "text/plain", "Error loading config"); // Handle the case when loading the config fails
                }
                //request->redirect("/");
        });

        server.on("/data", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            
                String strCnf = Esp32::configString_; 
                Serial.print("Sending data to client: ");
                Serial.println(strCnf);
               
                request->send(SPIFFS, "/setup.html", String(), false, processor);
          
        });

       
        server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request){
            Serial.println("Led ON requested");
            digitalWrite(BUILTIN_LED, HIGH);
            request->send(SPIFFS, "/index.html", String(),false, processor);
        });
        server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request){
            Serial.println("Led OFF requested");
            digitalWrite(BUILTIN_LED, LOW);
            request->send(SPIFFS, "/index.html", String(),false, processor);
        });



        server.on("/speedUp", HTTP_GET, [](AsyncWebServerRequest *request){
            digitalWrite(BUILTIN_LED, LOW);
            request->send(SPIFFS, "/index.html", String(),false, processor);
        });
        server.on("/slowDown", HTTP_GET, [](AsyncWebServerRequest *request){
            digitalWrite(BUILTIN_LED, LOW);
            request->send(SPIFFS, "/index.html", String(),false, processor);
        });

        // Generic I/O Configuration Endpoints
        server.on("/api/io/status", HTTP_GET, [](AsyncWebServerRequest *request){
            String status_json = Esp32::getIOStatusJsonString();
            request->send(200, "application/json", status_json);
        });

        server.on("/api/io/config", HTTP_GET, [](AsyncWebServerRequest *request){
            if (SPIFFS.exists(Esp32::CONFIG_IO_FILENAME)) {
                File file = SPIFFS.open(Esp32::CONFIG_IO_FILENAME, "r");
                if (file && !file.isDirectory()) {
                    request->send(file, Esp32::CONFIG_IO_FILENAME, "application/json");
                    // File is closed by the send() method
                    return;
                } else {
                    Serial.println(F("Error: Failed to open /io_config.json for reading."));
                    request->send(500, "application/json", "{\"status\":\"error\", \"message\":\"Error reading I/O config file.\"}");
                }
            } else {
                // If no config file exists, send a default valid empty structure
                request->send(200, "application/json", "{\"io_pins\":[]}");
            }
        });

        server.on("/api/io/config", HTTP_POST,
            [](AsyncWebServerRequest *request){
                // Not used for JSON body, but required by API
            },
            NULL, // onUpload handler
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                static String body_content_post_io; // Distinct static variable

                if (index == 0) {
                    body_content_post_io = "";
                    if (request->contentType() != "application/json") {
                       request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid Content-Type, must be application/json\"}");
                       // To fully stop processing for this request, you might need more, like request->client()->stop();
                       // For simplicity, we'll let it try to parse, which will likely fail if not JSON.
                    }
                }

                for (size_t i = 0; i < len; i++) {
                    body_content_post_io += (char)data[i];
                }

                if (index + len == total) {
                    Serial.println(F("WWW: Received new I/O config via POST /api/io/config:"));
                    Serial.println(body_content_post_io);

                    JsonDocument doc; // V7 uses dynamic allocation by default
                    DeserializationError error = deserializeJson(doc, body_content_post_io);
                    body_content_post_io = ""; // Clear static buffer

                    if (error) {
                        Serial.print(F("WWW Error: deserializeJson() failed for /api/io/config: "));
                        Serial.println(error.f_str());
                        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON format.\"}");
                        return;
                    }

                    if (Esp32::saveAndApplyIOConfiguration(doc)) {
                        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"I/O Configuration saved and applied.\"}");
                    } else {
                        request->send(500, "application/json", "{\"status\":\"error\", \"message\":\"Failed to save or apply I/O configuration.\"}");
                    }
                }
            }
        );

        server.on("/api/io/toggle", HTTP_POST, [](AsyncWebServerRequest *request){
            int gpio = -1;
            if (request->hasParam("gpio")) {
                gpio = request->getParam("gpio")->value().toInt();
            }

            if (gpio != -1) {
                if (Esp32::togglePin(gpio)) {
                    request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"GPIO " + String(gpio) + " toggled.\"}");
                } else {
                    request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Failed to toggle GPIO " + String(gpio) + ". Not a configured digital output or not found?\"}");
                }
            } else {
                request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing 'gpio' parameter.\"}");
            }
        });

        server.on("/api/basicmode/status", HTTP_GET, [](AsyncWebServerRequest *request){
            #ifdef BASIC_MODE
                JsonDocument statusDoc;
                // Placeholder for actual getter calls - these will be implemented in Step 5
                // bool led1_st = BasicMode::basicMode.getLed1State();
                // bool led2_st = BasicMode::basicMode.getLed2State();
                // bool led3_st = BasicMode::basicMode.getLed3State();

                // For now, using placeholder values until getters are ready
                // In a real scenario, if BasicMode isn't active, this endpoint might error or return default false.
                // However, BASIC_MODE ifdef should prevent access if mode isn't compiled.
                statusDoc["led1"] = BasicMode::basicMode.getLed1State(); // Assumes getter exists
                statusDoc["led2"] = BasicMode::basicMode.getLed2State(); // Assumes getter exists
                statusDoc["led3"] = BasicMode::basicMode.getLed3State(); // Assumes getter exists

                String jsonResponse;
                serializeJson(statusDoc, jsonResponse); // Using serializeJson directly
                request->send(200, "application/json", jsonResponse);
            #else
                // If BASIC_MODE is not defined, this endpoint theoretically shouldn't be hit
                // if the front-end is also conditional. But as a fallback:
                request->send(404, "text/plain", "Basic Mode not active");
            #endif
        });

        // General config endpoint (ensure it doesn't conflict if path is similar)
        server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            
            JsonDocument jsonBuffer;

            auto error = deserializeJson(jsonBuffer, (const char*)data);
            if(error)
            {
                Serial.print(F("deserializeJson() failed with code "));
                Serial.println(error.c_str());
                return;
            }else{
                Serial.println("Saving received config: ");
                String strCnf = JsonTools::getJsonString(jsonBuffer, true); // Changed to JsonTools
                Serial.println(strCnf);
                Esp32::saveConfig(jsonBuffer);  //  saves new config json on SPIFF and 

                request->send(200, "text/plain",strCnf);
            }/* else {
            request->send(404, "text/plain", "");
            }*/
        });
        server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
            Esp32::reboot();
        });

        
        

#ifdef HOCKEY_MODE       
        // Respond to /sensors route
        server.on("/hockey/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
            int val34 = analogRead(34);
            int val39 = analogRead(39);
            int val5  = digitalRead(5);

            String json = "{";
            json += "\"pin34\":" + String(val34) + ",";
            json += "\"pin39\":" + String(val39) + ",";
            json += "\"pin5\":"  + String(val5);
            json += "}";

            request->send(200, "application/json", json);
        });

        server.on("/hockey/", [](AsyncWebServerRequest *request){
            request->send(200, "text/html", "ESP32 Hockey Timer Interface");
        });
        server.on("/hockey/pause", HTTP_POST, [](AsyncWebServerRequest *request){
                if (!isPaused) {
                    pausedTime = millis() - startTime;
                    isPaused = true;
                } else {
                    startTime = millis() - pausedTime;
                    isPaused = false;
                }
                hockey.pause();
            request->send(200, "text/plain", "paused");
        });
        server.on("/hockey/reset", HTTP_POST,[](AsyncWebServerRequest *request){
            handleReset();
            request->send(200, "text/plain", "reset");
        }); 
        server.on("/hockey/setPeriodLength", HTTP_POST,[](AsyncWebServerRequest *request){
            Serial.println("Set period length requested");
            Serial.println(request->arg("minutes"));
            hockey.setPeriodLength(request->arg("minutes").toInt());
            hockey.reset();
            Esp32::saveConfig(Esp32::configJson_, false);
            request->send(200, "text/html", "ESP32 Hockey Timer Interface");
        }); 

        server.on("/hockey/setDelta", HTTP_POST, [](AsyncWebServerRequest *request){
            String side = "";
            int value = 0;
            bool success = false;

            if (request->hasParam("side")) { // Check for URL query parameter
                side = request->getParam("side")->value();
            } else {
                request->send(400, "text/plain", "Missing 'side' parameter.");
                return;
            }

            if (request->hasParam("value")) { // Check for URL query parameter
                value = request->getParam("value")->value().toInt();
            } else {
                request->send(400, "text/plain", "Missing 'value' parameter.");
                return;
            }

            if (side == "left") {
                Hockey::senseLeft.setDetection(value); // Corrected method name
                success = true;
            } else if (side == "right") {
                Hockey::senseRight.setDetection(value); // Corrected method name
                success = true;
            } else {
                request->send(400, "text/plain", "Invalid 'side' parameter. Must be 'left' or 'right'.");
                return;
            }

            if (success) {
                Esp32::saveConfig(Esp32::configJson_, false); // Persist the change
                request->send(200, "text/plain", "Delta for " + side + " set to " + String(value) + " and configuration saved.");
            }
        });

        server.on("/hockey/setIntroDuration", HTTP_POST, [](AsyncWebServerRequest *request){
            if (request->hasParam("duration_s")) {
                String durationSecondsStr = request->getParam("duration_s")->value();
                unsigned long durationMs = durationSecondsStr.toInt() * 1000UL;
                hockey.setIntroDurationMs(durationMs);
                Esp32::saveConfig(Esp32::configJson_, false);
                request->send(200, "text/plain", "Intro duration set to " + durationSecondsStr + " seconds.");
            } else {
                request->send(400, "text/plain", "Missing 'duration_s' parameter.");
            }
        });

        server.on("/hockey/setGoalCelebrationDuration", HTTP_POST, [](AsyncWebServerRequest *request){
            if (request->hasParam("duration_s")) {
                String durationSecondsStr = request->getParam("duration_s")->value();
                unsigned long durationMs = durationSecondsStr.toInt() * 1000UL;
                hockey.setGoalCelebrationMs(durationMs);
                Esp32::saveConfig(Esp32::configJson_, false);
                request->send(200, "text/plain", "Goal celebration duration set to " + durationSecondsStr + " seconds.");
            } else {
                request->send(400, "text/plain", "Missing 'duration_s' parameter.");
            }
        });

        server.on("/hockey/setPuckDropDuration", HTTP_POST, [](AsyncWebServerRequest *request){
            if (request->hasParam("duration_s")) {
                String durationSecondsStr = request->getParam("duration_s")->value();
                unsigned long durationMs = durationSecondsStr.toInt() * 1000UL;
                hockey.setPuckDropMs(durationMs);
                Esp32::saveConfig(Esp32::configJson_, false);
                request->send(200, "text/plain", "Puck drop duration set to " + durationSecondsStr + " seconds.");
            } else {
                request->send(400, "text/plain", "Missing 'duration_s' parameter.");
            }
        });

        server.on("/hockey/setPeriodIntermissionDuration", HTTP_POST, [](AsyncWebServerRequest *request){
            if (request->hasParam("duration_s")) {
                String durationSecondsStr = request->getParam("duration_s")->value();
                unsigned long durationMs = durationSecondsStr.toInt() * 1000UL;
                hockey.setPeriodIntermissionMs(durationMs);
                Esp32::saveConfig(Esp32::configJson_, false);
                request->send(200, "text/plain", "Period intermission duration set to " + durationSecondsStr + " seconds.");
            } else {
                request->send(400, "text/plain", "Missing 'duration_s' parameter.");
            }
        });

        server.on("/hockey/scoreboard", HTTP_GET,[](AsyncWebServerRequest *request){
           /* unsigned long elapsed = isPaused ? pausedTime : millis() - startTime;
            unsigned int minutes = (elapsed / 1000) / 60;
            unsigned int seconds = (elapsed / 1000) % 60;
          
            Serial.println(hockey.getScoreString());
            char buffer[6];
            sprintf(buffer, "%02d:%02d", minutes, seconds);
            request->send(200, "text/plain", hockey.getScoreString());*/



            String json = "{";
            json += "\"scoreLeft\":" + String(hockey.getScoreLeft()) + ",";
            json += "\"scoreRight\":" + String(hockey.getScoreRight()) + ",";
            json += "\"time\":\""      + String(hockey.gettimeString()) + "\",";
            json += "\"period\":"  + String(hockey.getPeriod()) + ",";
            json += "\"periodLength\":"  + String(hockey.getPeriodLength()) + ","; // This is in minutes
            json += "\"gameStatus\":"  + String(hockey.getCurrentGameState()) + ",";
            json += "\"leftDeltaValue\":" + String(hockey.getLeftDelta()) + ",";
            json += "\"rightDeltaValue\":" + String(hockey.getRightDelta()) + ",";

            // Add new duration fields (as seconds)
            json += "\"introDurationSeconds\":" + String(hockey.getIntroDurationMs() / 1000) + ",";
            json += "\"goalCelebrationSeconds\":" + String(hockey.getGoalCelebrationMs() / 1000) + ",";
            json += "\"puckDropSeconds\":" + String(hockey.getPuckDropMs() / 1000) + ",";
            json += "\"periodIntermissionSeconds\":" + String(hockey.getPeriodIntermissionMs() / 1000);

            json += "}";

            request->send(200, "application/json", json);


        }); 
#endif


        //server.serveStatic("/esp32.css", SPIFFS, "/esp32.css").setCacheControl("no-cache");

        server
            .serveStatic("/", SPIFFS, "/")
            .setDefaultFile("index.html")
            .setCacheControl("no-cache")
            .setTemplateProcessor(processor);


        // Send 404 for not Found
        server.onNotFound( [](AsyncWebServerRequest *request) {
            request->send(404, "text/plain", "Not found");
        });

        server.begin();

        Serial.println(F(" completed."));
    }
   
}


