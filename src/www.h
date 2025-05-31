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

#include "api/Esp32.h"
#include "Hockey.h"



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
            processedVar.replace("%MQTTURL%", Mqtt::server_ip);
        }
        if (processedVar.indexOf("%CONFIG%") >= 0) {
            JsonDocument d = Esp32::configJson_;
            d["pass"] = ""; // Suppress password for security
            processedVar.replace("%CONFIG%", Esp32::getJsonString(d));
        }
     
        return processedVar; 
    }

    
   String processor(const String& var)   //  this will be called for every %PLACEHOLDER% in the served html file
    {

        if(var == "STATE")    {  return digitalRead(BUILTIN_LED) ? "ON" : "OFF";    }

        // if(var == "PRESSION") {  return String(boat.pressure);                   }
        if(var == "TIME")     {  return Esp32::hourglass.getDateTimeString();       }
        if(var == "TEMP")     {  return String(Esp32::getCPUTemp());                }

        if(var == "LON")      {  return String(Esp32::GPS::lon);                    }
        if(var == "LAT")      {  return String(Esp32::GPS::lat);                    }

        if(var == "LOCALIP")  {  return Esp32::wifiManager.getIPString();           }
        if(var == "SSID")     {  return String(Esp32::wifiManager.getSSID());       }


        if(var == "MQTTURL")   {  return Mqtt::server_ip;                            }
       
        if(var == "CONFIG")   {  
            JsonDocument d = Esp32::configJson_;
            d["pass"] = "";   //  suppress password for security.  this forces user to enter it back everytime.
            return Esp32::getJsonString(d); 
            
        }

        return String();
    }


 /*   void jsonPOST()   //  TODO : test function.  ca doit etre fini...
    {        
        String url;
        url = "https://" + server_name  + "/data"; //+ ":" + Esp32::port
        
        if (!http.begin(url)) {
            Serial.println("HTTP client failed to connect ...");
        }
        else {
            http.addHeader("content-type" , "application/json");
            http.addHeader("Connection" , "close");

            String data =  "{ \"name\":\"John\", \"age\":30, \"car\":null }";
 
            Serial.println(data);
     
            int httpResponseCode = http.POST(data); //Send the actual POST request
            

            // Note that if the value returned is lesser that zero, then an error occurred on the connection. If it is greater than zero, then it’s a standard HTTP code.
            if(httpResponseCode > 0) {

                String response = http.getString();  //Get the response to the request
            
                Serial.println(httpResponseCode);   //Print return code
                Serial.println(response);           //Print request answer
            } else {
                Serial.print("Error on sending POST: ");
                Serial.println(httpResponseCode); 
            }

            http.end();  //Free resources
        }
          
    }*/


    // hockey board
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
                    cnfStr = Esp32::getJsonString(cnf, true);
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
                String strCnf = Esp32::getJsonString(jsonBuffer, true); 
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

        
        


        //
        /* hockey */
        //
      
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
            handlePause();
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
            request->send(200, "text/html", "ESP32 Hockey Timer Interface");
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
            json += "\"periodLength\":"  + String(hockey.getPeriodLength()) + ",";
            json += "\"gameStatus\":"  + String(Hockey::state);
            json += "}";

            request->send(200, "application/json", json);


        }); 

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


