#pragma once
/* 
Add to platformio.ini:
    lib_deps =
        https://github.com/me-no-dev/ESPAsyncWebServer.git  ; required for www.h
*/
#include <Arduino.h>
#include <vector>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
//#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Boat.h"

#include "api/Esp32.h"



unsigned long cstring_to_ul(const char* str, char** end = nullptr, int base = 10)
{
    errno = 0; // Used to see if there was success or failure by overflow conditions

    auto ul = strtoul(str, end, base);

    if(errno != ERANGE)
    {
    return ul;
    }

    return ULONG_MAX;
}



namespace www
{

    HTTPClient http;  

    const uint serverPort = 80;
    
    AsyncWebServer server(serverPort);
   

 
    String processor(const String& var)   //  this will be called for every %PLACEHOLDER% in the served html file
    {

        if(var == "STATE")    {  return digitalRead(BUILTIN_LED) ? "ON" : "OFF";   }
            //String ledState = digitalRead(BUILTIN_LED) ? "ON" : "OFF";   
            //return ledState;

        if(var == "PRESSION") {  return String(boat.pressure);                     }
        if(var == "TIME")     {  return Esp32::hourglass.getDateTimeString();      }
        if(var == "SPEED")    {  return String(boat.getSpeed());                   }
        if(var == "DIR")      {  return String(boat.getDir());                     }
        if(var == "TEMP")     {  return String(Esp32::getCPUTemp());               }

        if(var == "LOCALIP")  {  return String(Esp32::wifiManager.getIPString());  }
        if(var == "SSID")     {  return String(Esp32::wifiManager.getSSID());      }
        if(var == "MQTTIP")   {  return Mqtt::server_ip.toString();                }
        if(var == "IP0")      {  return String(Mqtt::server_ip[0]);                }
        if(var == "IP1")      {  return String(Mqtt::server_ip[1]);                }
        if(var == "IP2")      {  return String(Mqtt::server_ip[2]);                }
        if(var == "IP3")      {  return String(Mqtt::server_ip[3]);                }

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

    void IPtoEEPROM(IPAddress IPaddr) 
    {
        Serial.print("Saving IP to EEPROM : ");
        Serial.println(IPaddr);

        /*for (int n = 0; n < sizeof(ipchar); n++) // automatically adjust for number of digits        {
            EEPROM.write(n + startingAddress, ipchar[n]);
        }*/
        for (int n = 0; n < 4; n++) // automatically adjust for number of digits
        {
            EEPROM.write(n , IPaddr[n]);
            Serial.println( IPaddr[n]);
        }

        EEPROM.commit() ? Serial.println(F("\nEEPROM successfully committed")) : Serial.println(F("\nERROR! EEPROM commit failed"));
    }


    
    void setup() 
    {
        Serial.print(F("WWW from SPIFFS setup")); 

        // Initialize SPIFFS
        if(!SPIFFS.begin(true)){
            Serial.println(F("An Error has occurred while mounting SPIFFS"));
            return;
        }

        server
            .serveStatic("/", SPIFFS, "/")
            .setDefaultFile("index.html")
            .setTemplateProcessor(processor);

        server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
                String data =  "{ \"name\":\"John\", \"age\":30, \"car\":null }";
                request->send(200, "application/json", data);
                //request->redirect("/");
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
        server.on("/mqttip", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL
            , [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            
            DynamicJsonDocument jsonBuffer(1024);
            auto error = deserializeJson(jsonBuffer, (const char*)data);
            if(error)
            {
                Serial.print(F("deserializeJson() failed with code "));
                Serial.println(error.c_str());
                return;
            }else{
                //const char* ip[4];
           
                IPAddress mqttservIP(jsonBuffer["ip0"], jsonBuffer["ip1"], jsonBuffer["ip2"], jsonBuffer["ip3"]);
              /*  ip[0] = jsonBuffer["ip0"];
                ip[1] = jsonBuffer["ip1"];
                ip[2] = jsonBuffer["ip2"];
                ip[3] = jsonBuffer["ip3"];*/
                Serial.println(F("Receiving new IP Address: "));
                Serial.println(mqttservIP);
                const char* port = jsonBuffer["port"];

                IPtoEEPROM(mqttservIP);

                int p = cstring_to_ul(port);
                Serial.println(p);
                Mqtt::setup();
                request->send(200, "text/plain", mqttservIP.toString());
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


