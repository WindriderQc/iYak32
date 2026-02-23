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
#include "api/SystemLog.h" // For sensor history & error log APIs
#include "api/devices/BMX280.h" // For self-test and sensor data
#include "api/WifiManager.h" // For WiFi scan/status APIs

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

        if(var == "PRESSION") {  return BMX280::isSuccessfullyInitialized() ? String(BMX280::getPressure(), 1) : String("N/A");  }
        if(var == "SPEED")    {  return String("N/A");  }
        if(var == "DIR")      {  return String("N/A");  }

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
                // Check if pin exists in configured_pins and provide specific error
                bool found = false;
                for (const Esp32::IO_Pin_Detail& pin : Esp32::configured_pins) {
                    if (pin.gpio == gpio) {
                        found = true;
                        if (pin.type_str != "DIGITAL") {
                            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"GPIO " + String(gpio) + " is type '" + pin.type_str + "', not DIGITAL. Cannot toggle.\"}");
                            return;
                        }
                        if (pin.mode_str != "OUTPUT") {
                            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"GPIO " + String(gpio) + " is mode '" + pin.mode_str + "', not OUTPUT. Cannot toggle.\"}");
                            return;
                        }
                        break;
                    }
                }
                if (!found) {
                    request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"GPIO " + String(gpio) + " not found in configured pins (" + String(Esp32::configured_pins.size()) + " pins configured).\"}");
                    return;
                }
                if (Esp32::togglePin(gpio)) {
                    request->send(200, "application/json", "{\"status\":\"success\", \"message\":\"GPIO " + String(gpio) + " toggled.\"}");
                } else {
                    request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Failed to toggle GPIO " + String(gpio) + ".\"}");
                }
            } else {
                request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing 'gpio' parameter.\"}");
            }
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
                static String body_content_config;

                if (index == 0) {
                    body_content_config = "";
                    if (total > (size_t)Esp32::CONFIG_FILE_MAX_SIZE) {
                        request->send(413, "application/json", "{\"error\":\"Config payload too large\"}");
                        return;
                    }
                    body_content_config.reserve(total);
                }

                for (size_t i = 0; i < len; i++) {
                    body_content_config += (char)data[i];
                }

                if (index + len == total) {
                    JsonDocument jsonBuffer;
                    auto error = deserializeJson(jsonBuffer, body_content_config);
                    body_content_config = "";

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
                            // Preserve WiFi password if posted empty
                            String postedPass = jsonBuffer["pass"].as<String>();
                            postedPass.trim();
                            if (postedPass.isEmpty()) {
                                String existingPass = currentConfig["pass"].as<String>();
                                existingPass.trim();
                                if (!existingPass.isEmpty()) {
                                    jsonBuffer["pass"] = existingPass;
                                    Serial.println(F("WWW: Empty pass received; preserving existing saved password."));
                                }
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

                    Serial.println(F("Saving received config:"));
                    String strCnf = JsonTools::getJsonString(jsonBuffer, true);
                    Serial.println(strCnf);
                    Esp32::saveConfig(jsonBuffer);

                    request->send(200, "application/json", strCnf);
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
            doc["version"] = ver;
            if (BMX280::isSuccessfullyInitialized()) {
                doc["bmx_temp"] = BMX280::getTemperature();
                doc["bmx_pressure"] = BMX280::getPressure();
                doc["bmx_humidity"] = BMX280::getHumidity();
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
            doc["connected"] = Mqtt::mqttClient.connected();
            doc["rc"] = Mqtt::mqttClient.state();
            doc["attempts"] = Mqtt::connectAttemptCount_;
            doc["backoff_ms"] = Mqtt::backoffDuration_;
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });

        // 3. MQTT Force Reconnect
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
                static String wifi_body;
                if (index == 0) wifi_body = "";
                for (size_t i = 0; i < len; i++) wifi_body += (char)data[i];
                if (index + len == total) {
                    JsonDocument doc;
                    auto err = deserializeJson(doc, wifi_body);
                    wifi_body = "";
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
                if (Mqtt::mqttClient.connected()) {
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
                static String sound_body;
                if (index == 0) sound_body = "";
                for (size_t i = 0; i < len; i++) sound_body += (char)data[i];
                if (index + len == total) {
                    #ifdef BASIC_MODE
                        JsonDocument doc;
                        auto err = deserializeJson(doc, sound_body);
                        sound_body = "";
                        if (err) {
                            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
                            return;
                        }
                        bool enabled = doc["enabled"] | false;
                        BasicMode::basicMode.setSoundEnabled(enabled);
                        request->send(200, "application/json", "{\"status\":\"ok\",\"enabled\":" + String(enabled ? "true" : "false") + "}");
                    #else
                        sound_body = "";
                        request->send(404, "application/json", "{\"error\":\"Basic Mode not active\"}");
                    #endif
                }
            }
        );

        // 13. BasicMode Threshold Control
        server.on("/api/basicmode/threshold", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                static String thresh_body;
                if (index == 0) thresh_body = "";
                for (size_t i = 0; i < len; i++) thresh_body += (char)data[i];
                if (index + len == total) {
                    #ifdef BASIC_MODE
                        JsonDocument doc;
                        auto err = deserializeJson(doc, thresh_body);
                        thresh_body = "";
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
                        thresh_body = "";
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
            delay(200);
            Esp32::reboot();
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


