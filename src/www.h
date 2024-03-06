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



namespace www
{

    HTTPClient http;  

    const uint serverPort = 80;
    
    AsyncWebServer server(serverPort);
   

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


        if(var == "MQTTIP")   {  return Mqtt::server_ip.toString();                 }
        if(var == "IP0")      {  return String(Mqtt::server_ip[0]);                 }
        if(var == "IP1")      {  return String(Mqtt::server_ip[1]);                 }
        if(var == "IP2")      {  return String(Mqtt::server_ip[2]);                 }
        if(var == "IP3")      {  return String(Mqtt::server_ip[3]);                 }
        if(var == "CONFIG")   {  
            JsonDocument d = Esp32::configJson_;
            d["pass"] = "";   //  suppress password for security.  this forces user to enter it back everytime.
            return Esp32::getJsonString(d, true);     
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




    
    void setup() 
    {
        Serial.print(F("WWW from SPIFFS setup")); 

        server
            .serveStatic("/", SPIFFS, "/")
            .setDefaultFile("index.html")
            .setTemplateProcessor(processor);

        server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
                JsonDocument cnf;
                String cnfStr = "";
                Serial.println("Loading config from json: ");
                if(Esp32::loadConfig(&cnf)) {  
                    cnfStr = Esp32::getJsonString(cnf, true);
                    Serial.println(cnfStr);
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
                Serial.println("Saving receved config: ");
                String strCnf = Esp32::getJsonString(jsonBuffer, true); 
                Serial.println(strCnf);
                Esp32::saveConfig(jsonBuffer, false);  //  saves new config json and  dont REBOOT device

                request->send(200, "text/plain",strCnf);
            }/* else {
            request->send(404, "text/plain", "");
            }*/
        });

        





        // Send 404 for not Found
        server.onNotFound( [](AsyncWebServerRequest *request) {
            request->send(404, "text/plain", "Not found");
        });

        server.begin();

        Serial.println(F(" completed."));
    }
   
}


