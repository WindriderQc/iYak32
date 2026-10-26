#pragma once
#include "api/IOConfig.h"
#include "api/IOOutput.h"
/* 
Add to platformio.ini:
    lib_deps =
        https://github.com/me-no-dev/ESPAsyncWebServer.git  ; required for www.h
*/
//#include <vector>
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <cstdlib>
#include <cstring>
#include <SPIFFS.h>      // Added for SPIFFS object usage
#include "api/Mqtt.h"    // Added for Mqtt namespace usage
#include "api/Esp32.h"
#include "api/JsonTools.h" // Added for JsonTools::getJsonString
#include "api/SystemLog.h" // For sensor history & error log APIs
#include "api/devices/BMX280.h" // For self-test and sensor data
#include "api/WifiManager.h" // For WiFi scan/status APIs
#include "api/WifiConfig.h"

extern const char* ver; // Firmware version string from main.cpp

#ifdef HOCKEY_MODE // Assuming HOCKEY_MODE is defined in main.cpp or a config
    #include "Hockey.h"
#endif

#ifdef BASIC_MODE // Assuming BASIC_MODE is defined in main.cpp or a config
    #include "BasicMode.h"
#endif



namespace www
{

    HTTPClient http;

    void redactConfigSecrets(JsonDocument& doc) {
        bool tidesKeySet = !doc["tides_api_key"].as<String>().isEmpty();
        for (const char* key : {"pass", "wifi_password", "mqttuser", "mqtt_user", "mqttpass", "mqtt_pass", "tides_api_key"}) {
            if (!doc[key].isNull()) doc[key] = "";
        }
        doc["tides_api_key_set"] = tidesKeySet;
    }

    bool receiveBody(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                     size_t index, size_t total, size_t maxSize, String& result) {
        if (total > maxSize || index > total || len > total - index) {
            if (index == 0) request->send(413, "application/json", "{\"error\":\"Payload too large\"}");
            return false;
        }
        if (index == 0) {
            if (request->_tempObject != nullptr) {
                request->send(500, "application/json", "{\"error\":\"Request state error\"}");
                return false;
            }
            request->_tempObject = malloc(total + 1);
            if (request->_tempObject == nullptr) {
                request->send(503, "application/json", "{\"error\":\"Out of memory\"}");
                return false;
            }
        }
        char* buffer = static_cast<char*>(request->_tempObject);
        if (buffer == nullptr) return false;
        if (len > 0) memcpy(buffer + index, data, len);
        if (index + len != total) return false;
        buffer[total] = '\0';
        result = String(buffer);
        free(buffer);
        request->_tempObject = nullptr;
        return true;
    }

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
            redactConfigSecrets(d);
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
            redactConfigSecrets(d);
            return JsonTools::getJsonString(d); // Changed to JsonTools
        }

        if(var == "PRESSION") {  return BMX280::isSuccessfullyInitialized() ? String(BMX280::getPressure(), 1) : String("N/A");  }
        if(var == "SPEED")    {  return String("N/A");  }
        if(var == "DIR")      {  return String("N/A");  }

        return String();
    }

    void sendTemplatePage(AsyncWebServerRequest* request, const char* path) {
        File file = SPIFFS.open(path, FILE_READ);
        if (!file) {
            request->send(404, "text/plain", "Not found");
            return;
        }
        String html = file.readString();
        file.close();
        // Replace known tokens only. Ordinary CSS percentages stay untouched.
        for (const char* token : {"STATE", "TIME", "TEMP", "LON", "LAT", "LOCALIP", "SSID",
                                  "MQTTURL", "CONFIG", "PRESSION", "SPEED", "DIR"}) {
            html.replace(String("%") + token + "%", processor(token));
        }
        request->send(200, "text/html", html);
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
                    redactConfigSecrets(cnf);
                    cnfStr = JsonTools::getJsonString(cnf, true);
                    request->send(200, "application/json", cnfStr); // Send the JSON response with the appropriate content type
                } else {
                    request->send(500, "text/plain", "Error loading config"); // Handle the case when loading the config fails
                }
                //request->redirect("/");
        });

        server.on("/data", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            
                sendTemplatePage(request, "/setup.html");
          
        });

       
        server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request){
            Serial.println("Led ON requested");
            if (!Esp32::setPinActive(BUILTIN_LED, true)) { request->send(400, "text/plain", "LED GPIO is not a configured output"); return; }
            request->send(SPIFFS, "/index.html", String(),false, processor);
        });
        server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request){
            Serial.println("Led OFF requested");
            if (!Esp32::setPinActive(BUILTIN_LED, false)) { request->send(400, "text/plain", "LED GPIO is not a configured output"); return; }
            request->send(SPIFFS, "/index.html", String(),false, processor);
        });



        server.on("/speedUp", HTTP_GET, [](AsyncWebServerRequest *request){
            if (!Esp32::setPinActive(BUILTIN_LED, false)) { request->send(400, "text/plain", "LED GPIO is not a configured output"); return; }
            request->send(SPIFFS, "/index.html", String(),false, processor);
        });
        server.on("/slowDown", HTTP_GET, [](AsyncWebServerRequest *request){
            if (!Esp32::setPinActive(BUILTIN_LED, false)) { request->send(400, "text/plain", "LED GPIO is not a configured output"); return; }
            request->send(SPIFFS, "/index.html", String(),false, processor);
        });

        // Generic I/O Configuration Endpoints
        server.on("/api/io/status", HTTP_GET, [](AsyncWebServerRequest *request){
            String status_json = Esp32::getIOStatusJsonString();
            request->send(200, "application/json", status_json);
        });

        server.on("/api/io/events", HTTP_GET, [](AsyncWebServerRequest *request){
            String text = request->hasParam("after") ? request->getParam("after")->value() : "0";
            uint32_t after = strtoul(text.c_str(), nullptr, 10);
            if (text != String(after)) {
                request->send(400, "application/json", "{\"message\":\"after doit être un entier positif ou zéro.\"}"); return;
            }
            String session = request->hasParam("session") ? request->getParam("session")->value() : "";
            request->send(200, "application/json", Esp32::getIOEventsJsonString(after, session));
        });

        server.on("/api/io/config", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "application/json", Esp32::getIOConfigurationJsonString());
        });
        server.on("/api/io/alerts/status", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", Esp32::getIOAlertsStatusJson());
        });
        server.on("/api/io/activity", HTTP_GET, [](AsyncWebServerRequest* request) {
            String text = request->hasParam("after") ? request->getParam("after")->value() : "0";
            uint32_t after = strtoul(text.c_str(), nullptr, 10);
            if (text != String(after)) { request->send(400, "application/json", "{\"message\":\"after doit être un entier positif ou zéro.\"}"); return; }
            request->send(200, "application/json", Esp32::getIOActivityJson(after, request->hasParam("session") ? request->getParam("session")->value() : ""));
        });

        server.on("/api/io/automation/status", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", Esp32::getIOAutomationStatusJson());
        });

        server.on("/api/io/override", HTTP_POST, [](AsyncWebServerRequest *request) {
            if (!request->hasParam("gpio") || !request->hasParam("duration_ms")) { request->send(400, "application/json", "{\"message\":\"gpio et duration_ms requis.\"}"); return; }
            String gpioText = request->getParam("gpio")->value(), text = request->getParam("duration_ms")->value();
            int gpio = gpioText.toInt(); uint32_t ms = strtoul(text.c_str(), nullptr, 10);
            if (gpioText != String(gpio) || text != String(ms) || !IOConfig::available(gpio)) { request->send(400, "application/json", "{\"message\":\"GPIO et durée entiers requis.\"}"); return; }
            String json = Esp32::setIOOverrideJson(gpio, ms); JsonDocument result; deserializeJson(result, json);
            request->send(result["status"] == "success" ? 200 : 400, "application/json", json);
        });

        server.on("/api/io/zone/off", HTTP_POST, [](AsyncWebServerRequest*) {}, NULL,
            [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                String body; if (!receiveBody(request, data, len, index, total, 2048, body)) return;
                JsonDocument command; bool conflict = false;
                if (deserializeJson(command, body) || !command["zone"].is<const char*>() || !command["hold_ms"].is<uint32_t>()) { request->send(400, "application/json", "{\"message\":\"Zone et durée de priorité requises.\"}"); return; }
                String json = Esp32::stopIOZoneJson(command["zone"].as<String>(), command["_revision"] | "", command["hold_ms"], &conflict);
                JsonDocument result; deserializeJson(result, json); request->send(conflict ? 409 : result["status"] == "success" ? 200 : 400, "application/json", json);
            });

        server.on("/api/io/zone/resume", HTTP_POST, [](AsyncWebServerRequest*) {}, NULL,
            [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                String body; if (!receiveBody(request, data, len, index, total, 2048, body)) return;
                JsonDocument command; bool conflict = false;
                if (deserializeJson(command, body) || !command["zone"].is<const char*>()) { request->send(400, "application/json", "{\"message\":\"Zone requise.\"}"); return; }
                String json = Esp32::resumeIOZoneJson(command["zone"].as<String>(), command["_revision"] | "", &conflict);
                JsonDocument result; deserializeJson(result, json); request->send(conflict ? 409 : result["status"] == "success" ? 200 : 400, "application/json", json);
            });

        server.on("/api/io/automation/preview", HTTP_POST, [](AsyncWebServerRequest*) {}, NULL,
            [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                String body; if (!receiveBody(request, data, len, index, total, 40000, body)) return;
                JsonDocument command;
                if (deserializeJson(command, body) || !command["id"].is<const char*>() || !command["config"].is<JsonObject>()) { request->send(400, "application/json", "{\"message\":\"Identifiant et configuration du brouillon requis.\"}"); return; }
                JsonDocument config; config.set(command["config"]);
                String json = Esp32::previewIOAutomationJson(config, command["id"].as<String>()); JsonDocument result; deserializeJson(result, json);
                request->send(result["status"] == "success" ? 200 : 400, "application/json", json);
            });

        server.on("/api/io/config/validate", HTTP_POST, [](AsyncWebServerRequest*) {}, NULL,
            [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                String body;
                if (!receiveBody(request, data, len, index, total, 32768, body)) return;
                JsonDocument doc, result;
                const char* error = deserializeJson(doc, body) ? "JSON invalide." : IOConfig::validate(doc);
                result["status"] = error ? "error" : "valid";
                if (error) result["message"] = error;
                String json; serializeJson(result, json);
                request->send(error ? 400 : 200, "application/json", json);
            });

        server.on("/api/io/scene", HTTP_POST, [](AsyncWebServerRequest*) {}, NULL,
            [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                String body;
                if (!receiveBody(request, data, len, index, total, 2048, body)) return;
                JsonDocument doc;
                if (deserializeJson(doc, body) || !doc["id"].is<const char*>() || !doc["_revision"].is<const char*>() || doc["_revision"].as<String>().isEmpty()) {
                    request->send(400, "application/json", "{\"message\":\"id et _revision sont requis.\"}"); return;
                }
                bool conflict = false;
                String json = Esp32::runIOSceneJson(doc["id"].as<String>(), doc["_revision"].as<String>(), IOEvents::Source::WEB, &conflict);
                JsonDocument result; deserializeJson(result, json);
                request->send(conflict ? 409 : result["status"] == "success" ? 200 : 400, "application/json", json);
            });

        server.on("/api/io/config", HTTP_POST,
            [](AsyncWebServerRequest *request){
                // Not used for JSON body, but required by API
            },
            NULL, // onUpload handler
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                if (!request->contentType().startsWith("application/json")) {
                    if (index == 0) request->send(400, "application/json", "{\"error\":\"JSON required\"}");
                    return;
                }
                String body_content_post_io;
                if (!receiveBody(request, data, len, index, total, 32768, body_content_post_io)) return;
                {
                    Serial.println(F("WWW: Received new I/O config via POST /api/io/config:"));
                    Serial.println(body_content_post_io);

                    JsonDocument doc; // V7 uses dynamic allocation by default
                    DeserializationError error = deserializeJson(doc, body_content_post_io);

                    if (error) {
                        Serial.print(F("WWW Error: deserializeJson() failed for /api/io/config: "));
                        Serial.println(error.f_str());
                        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON format.\"}");
                        return;
                    }

                    if (const char* validationError = IOConfig::validate(doc)) {
                        JsonDocument result;
                        result["status"] = "error";
                        result["message"] = validationError;
                        String json;
                        serializeJson(result, json);
                        request->send(400, "application/json", json);
                        return;
                    }

                    bool conflict = false;
                    if (Esp32::saveAndApplyIOConfiguration(doc, &conflict)) {
                        request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"I/O Configuration saved and applied.\"}");
                    } else if (conflict) {
                        request->send(409, "application/json", "{\"status\":\"conflict\",\"message\":\"Configuration modifiée ailleurs. Actualisez et vérifiez vos changements.\"}");
                    } else {
                        request->send(500, "application/json", "{\"status\":\"error\", \"message\":\"Failed to save or apply I/O configuration.\"}");
                    }
                }
            }
        );

        server.on("/api/io/set", HTTP_POST, [](AsyncWebServerRequest *request){
            bool logical = request->hasParam("active");
            const char* field = logical ? "active" : "value";
            if (!request->hasParam("gpio") || !request->hasParam(field)) {
                request->send(400, "application/json", "{\"message\":\"gpio et active (ou value) sont requis.\"}"); return;
            }
            String gpioText = request->getParam("gpio")->value();
            String value = request->getParam(field)->value();
            int gpio = gpioText.toInt();
            uint32_t duration = 0;
            if (request->hasParam("duration_ms")) {
                String text = request->getParam("duration_ms")->value();
                duration = strtoul(text.c_str(), nullptr, 10);
                if (!logical || value != "1" || text != String(duration) || !IOOutput::validDuration(duration)) {
                    request->send(400, "application/json", "{\"message\":\"Durée valide : de 100 à 3600000 ms, avec active=1.\"}"); return;
                }
            }
            if (gpioText != String(gpio) || !IOConfig::available(gpio) || (value != "0" && value != "1")) {
                request->send(400, "application/json", "{\"message\":\"GPIO entier valide et état 0 ou 1 requis.\"}"); return;
            }
            String result = Esp32::setIOOutputJson(gpio, value == "1", logical, duration);
            if (result.isEmpty()) {
                request->send(400, "application/json", "{\"message\":\"Cette broche n'est pas une sortie numérique configurée.\"}"); return;
            }
            request->send(200, "application/json", result);
        });

        server.on("/api/io/toggle", HTTP_POST, [](AsyncWebServerRequest *request){
            String text = request->hasParam("gpio") ? request->getParam("gpio")->value() : "";
            int gpio = text.toInt();
            if (text != String(gpio) || !Esp32::togglePin(gpio)) {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Sortie numérique configurée requise.\"}"); return;
            }
            request->send(200, "application/json", "{\"status\":\"success\"}");
        });

        server.on("/api/basicmode/analogstatus", HTTP_GET, [](AsyncWebServerRequest *request){
            #ifdef BASIC_MODE
                JsonDocument statusDoc;
                statusDoc["analog1_value"] = BasicMode::basicMode.getAnalogValue1();
                statusDoc["analog1_threshold"] = BasicMode::basicMode.getAnalogThreshold1();
                statusDoc["analog2_value"] = BasicMode::basicMode.getAnalogValue2();
                statusDoc["analog2_threshold"] = BasicMode::basicMode.getAnalogThreshold2();

                String jsonResponse;
                serializeJson(statusDoc, jsonResponse);
                request->send(200, "application/json", jsonResponse);
            #else
                request->send(404, "application/json", "{\"error\":\"Basic Mode not active\"}");
            #endif
        });

        server.on("/api/basicmode/status", HTTP_GET, [](AsyncWebServerRequest *request){
            #ifdef BASIC_MODE
                JsonDocument statusDoc;
                statusDoc["led1"] = BasicMode::basicMode.getLed1State();
                statusDoc["led2"] = BasicMode::basicMode.getLed2State();
                statusDoc["led3"] = BasicMode::basicMode.getLed3State();
                if (!Esp32::isPinOutputAvailable(BasicMode::LED1_PIN)) statusDoc["led1"] = nullptr;
                if (!Esp32::isPinOutputAvailable(BasicMode::LED2_PIN)) statusDoc["led2"] = nullptr;
                if (!Esp32::isPinOutputAvailable(BasicMode::LED3_PIN)) statusDoc["led3"] = nullptr;
                statusDoc["sound"] = BasicMode::basicMode.isSoundEnabled();

                String jsonResponse;
                serializeJson(statusDoc, jsonResponse);
                request->send(200, "application/json", jsonResponse);
            #else
                request->send(404, "application/json", "{\"error\":\"Basic Mode not active\"}");
            #endif
        });

        // General config endpoint - accumulates chunked body before parsing
        server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                String body_content_config;
                if (!receiveBody(request, data, len, index, total, Esp32::CONFIG_FILE_MAX_SIZE, body_content_config)) return;
                {
                    JsonDocument jsonBuffer;
                    auto error = deserializeJson(jsonBuffer, body_content_config);

                    if (error) {
                        Serial.print(F("deserializeJson() failed: "));
                        Serial.println(error.c_str());
                        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                        return;
                    }

                    // Preserve fields from existing config that setup.html doesn't send
                    {
                        JsonDocument currentConfig;
                        if (Esp32::loadConfig(false, &currentConfig)) {
                            // Preserve WiFi password if omitted or posted blank.
                            if (WifiConfig::preservePassword(jsonBuffer, currentConfig)) {
                                Serial.println(F("WWW: Missing or empty pass received; preserving existing saved password."));
                            }
                            // Preserve tides_api_key (not editable from setup page)
                            if (jsonBuffer["tides_api_key"].isNull()) {
                                String existingKey = currentConfig["tides_api_key"] | "";
                                if (!existingKey.isEmpty()) {
                                    jsonBuffer["tides_api_key"] = existingKey;
                                    Serial.println(F("WWW: Preserving existing tides_api_key."));
                                }
                            }
                            // Preserve MQTT credentials (not editable from setup page)
                            if (jsonBuffer["mqttuser"].isNull()) {
                                String val = currentConfig["mqttuser"] | "";
                                if (!val.isEmpty()) jsonBuffer["mqttuser"] = val;
                            }
                            if (jsonBuffer["mqttpass"].isNull()) {
                                String val = currentConfig["mqttpass"] | "";
                                if (!val.isEmpty()) jsonBuffer["mqttpass"] = val;
                            }
                        }
                    }

                    if (!Esp32::saveConfig(jsonBuffer)) {
                        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Config save failed\"}");
                        return;
                    }
                    request->send(200, "application/json", "{\"status\":\"ok\"}");
                }
            }
        );
        server.on("/api/tides/key", HTTP_GET, [](AsyncWebServerRequest *request){
            String key = Esp32::configJson_["tides_api_key"] | "";
            request->send(200, "application/json", "{\"key\":\"" + key + "\"}");
        });

        // ===== NEW API ENDPOINTS =====

        // 1. Live System Status
        server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
            JsonDocument doc;
            doc["cpu_temp"] = Esp32::getCPUTemp();
            doc["cpu_freq"] = Esp32::getCPUFreq();
            doc["free_heap"] = Esp32::getRemainingHeap();
            doc["battery"] = Esp32::getBattRemaining(false);
            doc["wifi_rssi"] = Esp32::wifiManager.getWiFiStrength();
            doc["wifi_ssid"] = Esp32::wifiManager.getSSID();
            doc["wifi_ip"] = Esp32::wifiManager.getIPString();
            doc["wifi_connected"] = Esp32::wifiManager.isConnected();
            doc["mqtt_state"] = Mqtt::getStateString();
            doc["mqtt_enabled"] = Mqtt::isEnabled;
            doc["time"] = Esp32::hourglass.getDateTimeString();
            doc["uptime"] = millis() / 1000;
            doc["device_name"] = Esp32::DEVICE_NAME;
            doc["friendly_name"] = Esp32::getFriendlyName();
            doc["version"] = ver;
            doc["sample_interval_sec"] = Esp32::mqtt_data_interval_seconds_;
            if (SystemLog::getSensorCount()) doc["sensor_age_ms"] = millis() - SystemLog::getLastSensorMillis();
            else doc["sensor_age_ms"] = nullptr;
            #ifdef BASIC_MODE
                doc["mode"] = "basic";
            #elif defined(BOAT)
                doc["mode"] = "boat";
            #elif defined(HOCKEY_MODE)
                doc["mode"] = "hockey";
            #else
                doc["mode"] = "sensor";
            #endif
            if (BMX280::isSuccessfullyInitialized()) {
                doc["bmx_temp"] = BMX280::getTemperature();
                doc["bmx_pressure"] = BMX280::getPressure();
                if (BMX280::hasHumidity()) doc["bmx_humidity"] = BMX280::getHumidity();
            }
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });

        // 2. MQTT Status
        server.on("/api/mqtt/status", HTTP_GET, [](AsyncWebServerRequest *request){
            JsonDocument doc;
            doc["state"] = Mqtt::getStateString();
            doc["enabled"] = Mqtt::isEnabled;
            doc["server"] = Mqtt::serverIp;
            doc["port"] = Mqtt::port;
            doc["connected"] = Mqtt::getState() == Mqtt::MqttState::CONNECTED;
            doc["io_discovery_ready"] = Mqtt::ioDiscoveryReady;
            doc["io_discovery_count"] = Mqtt::ioDiscoveryCount;
            doc["automation_discovery_count"] = Mqtt::automationDiscoveryCount;
            doc["rc"] = Mqtt::mqttClient.state();
            doc["attempts"] = Mqtt::connectAttemptCount_;
            doc["backoff_ms"] = Mqtt::backoffDuration_;
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });

        // 3. MQTT Force Reconnect
        server.on("/api/mqtt/traffic", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "application/json", Mqtt::getTrafficJson());
        });
        server.on("/api/mqtt/reconnect", HTTP_POST, [](AsyncWebServerRequest *request){
            Mqtt::forceReconnect();
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Reconnect initiated\"}");
        });

        // 4. Sensor History Log
        server.on("/api/logs/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
            String json = SystemLog::getSensorLogJson();
            request->send(200, "application/json", json);
        });

        // 5. Error Log
        server.on("/api/logs/errors", HTTP_GET, [](AsyncWebServerRequest *request){
            String json = SystemLog::getErrorLogJson();
            request->send(200, "application/json", json);
        });

        // 6. Clear Error Log
        server.on("/api/logs/errors/clear", HTTP_POST, [](AsyncWebServerRequest *request){
            SystemLog::clearErrors();
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Error log cleared\"}");
        });

        // 7. WiFi Scan
        // Uses cached results from a previous async scan to avoid blocking the event loop.
        // First call triggers scan; client should retry after ~3 seconds.
        server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request){
            int n = WiFi.scanComplete();
            if (n == WIFI_SCAN_FAILED) {
                // No scan in progress — start one (async=true, show_hidden=false)
                WiFi.scanNetworks(true, false);
                request->send(200, "application/json", "{\"scanning\":true}");
                return;
            }
            if (n == WIFI_SCAN_RUNNING) {
                request->send(200, "application/json", "{\"scanning\":true}");
                return;
            }
            // Scan complete — return results
            JsonDocument doc;
            JsonArray arr = doc.to<JsonArray>();
            for (int i = 0; i < n && i < 20; i++) {
                JsonObject net = arr.add<JsonObject>();
                net["ssid"] = WiFi.SSID(i);
                net["rssi"] = WiFi.RSSI(i);
                net["enc"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
            }
            WiFi.scanDelete();
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });

        // 8. WiFi Connect
        server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                String wifi_body;
                if (!receiveBody(request, data, len, index, total, 256, wifi_body)) return;
                {
                    JsonDocument doc;
                    auto err = deserializeJson(doc, wifi_body);
                    if (err) {
                        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
                        return;
                    }
                    String ssid = doc["ssid"] | "";
                    String pass = doc["pass"] | "";
                    if (ssid.isEmpty()) {
                        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"SSID required\"}");
                        return;
                    }
                    // Update config and save
                    Esp32::configJson_["ssid"] = ssid;
                    Esp32::configJson_["pass"] = pass;
                    Esp32::saveConfig(Esp32::configJson_, false);
                    Esp32::wifiManager.setSSID(ssid);
                    Esp32::wifiManager.setPASS(pass);
                    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"WiFi config updated. Rebooting...\"}");
                    delay(500);
                    Esp32::reboot();
                }
            }
        );

        // 9. WiFi Status
        server.on("/api/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request){
            JsonDocument doc;
            doc["connected"] = Esp32::wifiManager.isConnected();
            doc["ssid"] = Esp32::wifiManager.getSSID();
            doc["ip"] = Esp32::wifiManager.getIPString();
            doc["rssi"] = Esp32::wifiManager.getWiFiStrength();
            doc["mode"] = WiFi.getMode() == WIFI_AP ? "AP" : "STA";
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });

        // 10. System Self-Test
        server.on("/api/system/selftest", HTTP_GET, [](AsyncWebServerRequest *request){
            JsonDocument doc;
            JsonArray arr = doc.to<JsonArray>();

            // SPIFFS test
            {
                JsonObject t = arr.add<JsonObject>();
                t["name"] = "SPIFFS Filesystem";
                if (Esp32::spiffsMounted) {
                    size_t total = SPIFFS.totalBytes();
                    size_t used = SPIFFS.usedBytes();
                    t["status"] = "pass";
                    t["detail"] = String(used/1024) + "KB / " + String(total/1024) + "KB used";
                } else {
                    t["status"] = "fail";
                    t["detail"] = "Not mounted";
                }
            }

            // WiFi test
            {
                JsonObject t = arr.add<JsonObject>();
                t["name"] = "WiFi Connection";
                if (Esp32::wifiManager.isConnected()) {
                    t["status"] = "pass";
                    t["detail"] = Esp32::wifiManager.getSSID() + " (" + String(Esp32::wifiManager.getWiFiStrength()) + " dBm)";
                } else {
                    t["status"] = "fail";
                    t["detail"] = "Disconnected";
                }
            }

            // MQTT test
            {
                JsonObject t = arr.add<JsonObject>();
                t["name"] = "MQTT Broker";
                if (Mqtt::getState() == Mqtt::MqttState::CONNECTED) {
                    t["status"] = "pass";
                    t["detail"] = Mqtt::serverIp + ":" + String(Mqtt::port);
                } else if (!Mqtt::isEnabled) {
                    t["status"] = "warn";
                    t["detail"] = "Disabled in config";
                } else {
                    t["status"] = "fail";
                    t["detail"] = String("State: ") + Mqtt::getStateString();
                }
            }

            // BMX280 test
            {
                JsonObject t = arr.add<JsonObject>();
                t["name"] = "BMX280 Sensor";
                if (BMX280::isSuccessfullyInitialized()) {
                    t["status"] = "pass";
                    t["detail"] = String(BMX280::getTemperature(), 1) + " C, " + String(BMX280::getPressure(), 0) + " hPa";
                } else {
                    t["status"] = "warn";
                    t["detail"] = "Not detected";
                }
            }

            // Heap test
            {
                JsonObject t = arr.add<JsonObject>();
                t["name"] = "Free Heap Memory";
                int heap = Esp32::getRemainingHeap();
                t["detail"] = String(heap / 1024) + " KB";
                if (heap > 50000) t["status"] = "pass";
                else if (heap > 20000) t["status"] = "warn";
                else t["status"] = "fail";
            }

            // CPU temp test
            {
                JsonObject t = arr.add<JsonObject>();
                t["name"] = "CPU Temperature";
                float temp = Esp32::getCPUTemp();
                t["detail"] = String(temp, 1) + " C";
                if (temp < 75) t["status"] = "pass";
                else if (temp < 85) t["status"] = "warn";
                else t["status"] = "fail";
            }

            // Battery test
            {
                JsonObject t = arr.add<JsonObject>();
                t["name"] = "Battery Voltage";
                float batt = Esp32::getBattRemaining(false);
                t["detail"] = String(batt, 2) + " V";
                if (batt == 0) t["status"] = "warn";
                else if (batt > 3.3) t["status"] = "pass";
                else if (batt > 3.0) t["status"] = "warn";
                else t["status"] = "fail";
            }

            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });

        // 11. System Uptime
        server.on("/api/system/uptime", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "application/json", "{\"uptime\":" + String(millis() / 1000) + "}");
        });

        // 12. BasicMode Sound Toggle
        server.on("/api/basicmode/sound", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                String sound_body;
                if (!receiveBody(request, data, len, index, total, 128, sound_body)) return;
                {
                    #ifdef BASIC_MODE
                        JsonDocument doc;
                        auto err = deserializeJson(doc, sound_body);
                        if (err) {
                            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
                            return;
                        }
                        bool enabled = doc["enabled"] | false;
                        BasicMode::basicMode.setSoundEnabled(enabled);
                        request->send(200, "application/json", "{\"status\":\"ok\",\"enabled\":" + String(enabled ? "true" : "false") + "}");
                    #else
                        request->send(404, "application/json", "{\"error\":\"Basic Mode not active\"}");
                    #endif
                }
            }
        );

        // 13. BasicMode Threshold Control
        server.on("/api/basicmode/threshold", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                String thresh_body;
                if (!receiveBody(request, data, len, index, total, 128, thresh_body)) return;
                {
                    #ifdef BASIC_MODE
                        JsonDocument doc;
                        auto err = deserializeJson(doc, thresh_body);
                        if (err) {
                            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
                            return;
                        }
                        int channel = doc["channel"] | 0;
                        int value = doc["value"] | 2000;
                        if (channel == 1) {
                            BasicMode::basicMode.setAnalogThreshold1(value);
                        } else if (channel == 2) {
                            BasicMode::basicMode.setAnalogThreshold2(value);
                        } else {
                            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid channel (1 or 2)\"}");
                            return;
                        }
                        request->send(200, "application/json", "{\"status\":\"ok\",\"channel\":" + String(channel) + ",\"value\":" + String(value) + "}");
                    #else
                        request->send(404, "application/json", "{\"error\":\"Basic Mode not active\"}");
                    #endif
                }
            }
        );

        // 14. Tides Configuration
        server.on("/api/config/tides", HTTP_GET, [](AsyncWebServerRequest *request){
            JsonDocument doc;
            doc["lat"] = Esp32::tides_lat_;
            doc["lon"] = Esp32::tides_lon_;
            String key = Esp32::configJson_["tides_api_key"] | "";
            if (!key.isEmpty()) doc["has_key"] = true;
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });

        // ===== END NEW API ENDPOINTS =====

        server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "Rebooting...");
            Esp32::requestReboot();
        });

        
        

#ifdef HOCKEY_MODE       
        // Respond to /sensors route
        server.on("/hockey/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
            JsonDocument doc;
            doc["pin34"] = analogRead(34);
            doc["pin39"] = analogRead(39);
            doc["pin5"]  = digitalRead(5);
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
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
            JsonDocument doc;
            doc["scoreLeft"] = hockey.getScoreLeft();
            doc["scoreRight"] = hockey.getScoreRight();
            doc["time"] = hockey.gettimeString();
            doc["period"] = hockey.getPeriod();
            doc["periodLength"] = hockey.getPeriodLength();
            doc["gameStatus"] = hockey.getCurrentGameState();
            doc["leftDeltaValue"] = hockey.getLeftDelta();
            doc["rightDeltaValue"] = hockey.getRightDelta();
            doc["introDurationSeconds"] = hockey.getIntroDurationMs() / 1000;
            doc["goalCelebrationSeconds"] = hockey.getGoalCelebrationMs() / 1000;
            doc["puckDropSeconds"] = hockey.getPuckDropMs() / 1000;
            doc["periodIntermissionSeconds"] = hockey.getPeriodIntermissionMs() / 1000;
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);


        }); 
#endif


        //server.serveStatic("/esp32.css", SPIFFS, "/esp32.css").setCacheControl("no-cache");

        server.on("/setup.html", HTTP_GET, [](AsyncWebServerRequest* request){
            sendTemplatePage(request, "/setup.html");
        });
        server.on("/boat/index.html", HTTP_GET, [](AsyncWebServerRequest* request){
            sendTemplatePage(request, "/boat/index.html");
        });
        server.on("/boat/", HTTP_GET, [](AsyncWebServerRequest* request){
            sendTemplatePage(request, "/boat/index.html");
        });

        server
            .serveStatic("/", SPIFFS, "/")
            .setDefaultFile("index.html")
            .setCacheControl("no-cache")
            .setFilter([](AsyncWebServerRequest* request) {
                String path = request->url();
                return !path.endsWith(".json") && !path.endsWith(".txt") && !path.endsWith(".tmp") && !path.endsWith(".bak");
            });


        // Send 404 for not Found
        server.onNotFound( [](AsyncWebServerRequest *request) {
            request->send(404, "text/plain", "Not found");
        });

        server.begin();

        Serial.println(F(" completed."));
    }
   
}

