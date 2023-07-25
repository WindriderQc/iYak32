#pragma once
/* 
Add to platformio.ini:
    lib_deps =
        https://github.com/me-no-dev/ESPAsyncWebServer.git  ; required for www.h
*/
#include <Arduino.h>
#include <vector>
#include <ESPAsyncWebServer.h>
//#include <AsyncTCP.h>
#include <SPIFFS.h>
//#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Boat.h"

//#include "api/Esp32.h"
//#include "BMX280.h"



namespace www
{

    HTTPClient http;  

    const uint serverPort = 80;
    
    AsyncWebServer server(serverPort);
   

 
    String processor(const String& var)   //  this will be called for every %PLACEHOLDER% in the served html file
    {

        Serial.println(var);

        String ledState;
        String t =".";
        String Spd;
        String Dir;
        String Pression ="test";
        String Temp;


        if(var == "STATE"){
            if(digitalRead(BUILTIN_LED))  
                ledState = "ON";
            else   
                ledState = "OFF";
            
            Serial.println(ledState);
            return ledState;
        }

        if(var == "TIME") {
            t = time(0);
            Serial.print(F("t: "));
            Serial.println(t.c_str());
            return t;
        }

        if(var == "SPEED") {
           // Spd = BoatStats::speed;
            return(String(boat.getSpeed()));
        }

        if(var == "DIR") {
          //  Dir = BoatStats::direction;
            return(String(boat.getDir()));
        }

        
        if(var == "TEMP") {
           // Temp =  "test";//BMX280::temp;
            return(String(Esp32::getCPUTemp()));
        }

        if(var == "PRESSION") {
          //  Pression = "test";//BMX280::pressure;
            return(Pression);
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
        Serial.print(F("WWW on SPIFFS setup")); 

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
        


        // Send 404 for not Found
        server.onNotFound( [](AsyncWebServerRequest *request) {
            request->send(404, "text/plain", "Not found");
        });

       
        server.begin();

        Serial.println(F(" completed."));
    }
    


    
    

}


