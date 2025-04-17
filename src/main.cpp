//#define VERBOSE
//#define HOCKEY
//#define BOAT

#include <Arduino.h>
#include "api/Esp32.h"
#include "api/devices/BMX280.h"  //  TODO : devrait etre encaps dans un device...   device = gestion IO = main config n feature focus
#include "api/devices/Oled.h"

#include "www.h"

#ifdef BOAT
    #include "Boat.h"
#endif

#ifdef HOCKEY  
    #include "Hockey.h"
#endif

const char* ver = "v1:7 ";




bool oledConnected = false;
bool bmxConnected = false;

//  STATE Machine - System 
enum class SYS_state {
    BOOT,
    DEVICES_CONFIG,
    HEATUP,
    FIRSTLOOP,
    LOOP
};
SYS_state state = SYS_state::BOOT;




void sendHeartbeat()
{
    std::vector<String> names;
    std::vector<String> values;

    names.push_back("sender");      values.push_back(Esp32::DEVICE_NAME);
    names.push_back("time");        values.push_back(Esp32::hourglass.getDateTimeString());

    #ifdef VERBOSE
        Mqtt::sendJson( names, values, "esp32/alive/" + Esp32::DEVICE_NAME, true);   
    #else
        Mqtt::sendJson( names, values, "esp32/alive/" + Esp32::DEVICE_NAME, false);               
    #endif
}

void sendData()
{
    std::vector<String> names;
    std::vector<String> values;

    names.push_back("sender");      values.push_back(Esp32::DEVICE_NAME);
    names.push_back("time");        values.push_back(Esp32::hourglass.getDateTimeString());
    names.push_back("CPUtemp");     values.push_back(String(Esp32::getCPUTemp()));    
    names.push_back("wifi");        values.push_back(String(Esp32::wifiManager.getWiFiStrength()));

    names.push_back("battery");     values.push_back(String(Esp32::getBattRemaining()));

    names.push_back("tempBM_280");  values.push_back(String(BMX280::temp));
    names.push_back("pressure");    values.push_back(String(BMX280::pressure));
    names.push_back("altitude");    values.push_back(String(BMX280::altitude));
    /*names.push_back("co2");         values.push_back(String(Weather::co2));
    names.push_back("smoke");       values.push_back(String(Weather::smoke));
    names.push_back("lpg");         values.push_back(String(Weather::lpg));
    names.push_back("airHumid");    values.push_back(String(Weather::airHumidity));
    names.push_back("tempDht");     values.push_back(String(Weather::tempDht));
    names.push_back("ir");          values.push_back(String(Lux::ir_));
    names.push_back("full");        values.push_back(String(Lux::full_));
    names.push_back("visible");     values.push_back(String(Lux::visible_));
    names.push_back("lux");         values.push_back(String(Lux::lux_));*/

    #ifdef VERBOSE
        Mqtt::sendJson( names, values, "esp32/data/" + Esp32::DEVICE_NAME, true);   
    #else
        Mqtt::sendJson( names, values, "esp32/data/" + Esp32::DEVICE_NAME, false);               
    #endif
}

void printOled()
{
    Oled::oled.clearDisplay();
              
    Oled::oled.setTextSize(1);             
    Oled::oled.setCursor(0,0);             // Start at top-left corner
    Oled::oled.setTextColor(WHITE);  
   // Oled::oled.printf("Speed:%d     %s\n",boat.getSpeed(), ver);            
   // Oled::oled.printf("Dir: %d\n", boat.getDir()); 
    Oled::oled.setTextSize(1);  
    Oled::oled.println(Esp32::wifiManager.getIP());
    Oled::oled.printf("\nKPa: %.2f", BMX280::pressure); 
    Oled::oled.printf("\ntC: %.1f", BMX280::temp);
    Oled::oled.printf("\nAlt: %.1f", BMX280::altitude);

    Oled::oled.display(); 
}




void setup() 
{
    Serial.begin(115200);
    Serial.print(F("\nLaunching initial Setup  ")); Serial.print(ver); Serial.print(F("  --   Device: ")); Serial.println(Esp32::DEVICE_NAME);

    static const char* TAG = "Main";
    esp_log_level_set(TAG,ESP_LOG_WARN); 
    #ifdef VERBOSE
        Esp32::setVerboseLog();
    #endif

    Esp32::configPin(LED_BUILTIN, "OUT", "Builtin LED");   //  TODO :  devrait retourner la pin ou ios [].  comme ca on utiliserait ca sinon pointless on dirait....  on prends encore LED_BUILTIN dans la next commant... :)
    Esp32::ioBlink(LED_BUILTIN,200, 200, 4);
    
    Esp32::setup();   // SPIFFS,  WifiManager,  OTA,  NTP,  MQTT,  Config,  GPIO,  ADC,  I2C,  SPI,  UART,  GPS,  Hourglass,  Buzzer,  etc...

    if(Esp32::spiffsMounted) www::setup(); //  Start the web server

    oledConnected = Oled::setupOled();    

    bmxConnected  = BMX280::init();                       
 
    Serial.println(F("Setup completed. - Launching State Machine...\n"));
}



void loop() 
{
    Esp32::loop(); //  Handle OTA,  MQTT,  NTP,  etc...
    

    switch(state) 
    {
    case SYS_state::BOOT:
            
            if(Mqtt::isEnabled)             Mqtt::mqttClient.publish("esp32/register", Esp32::DEVICE_NAME.c_str() ); //Once connected, publish an announcement...    
            if(Esp32::isConfigFromServer)   Mqtt::mqttClient.publish("esp32/config", Esp32::DEVICE_NAME.c_str());  // Request IO config and profile from server
            
            Serial.println("BOOT done");
            state = SYS_state::DEVICES_CONFIG;
            break;

    case SYS_state::DEVICES_CONFIG:

            //setupTimerz();
           // setupSensors();
            //setupAlarmsClock();

#ifdef BOAT  
            boat.setup(boat.SERVO_PIN, boat.RPWM, boat.LPWM, Esp32::ADC_Max);
            boat.setDir(135);
            boat.setSpeed(1,1,0); 
#endif

#ifdef HOCKEY  
            hockey.setup();
#endif

            Serial.println("SENSORS & ALARMLib CONFIG done");
            state = SYS_state::HEATUP;
            break;
    
    case SYS_state::HEATUP:

            Serial.print("Heating up...");
           // insure all sensor are heated up to send accurate data at start
            if(millis() <= 3000) {
                Serial.print("."); Serial.print(millis()); Serial.print("."); 
            }  
            else {
                state = SYS_state::FIRSTLOOP; Serial.println(" On Fire! :)");
                }
            break;

    case SYS_state::FIRSTLOOP:

            Serial.println();
            Esp32::getBattRemaining(true);
            if(bmxConnected) BMX280::actualizeWeather(true);

#ifdef HOCKEY  
            hockey.warmup();
#endif            
           
            //Lux::loop(); 
            
            Serial.println("FIRST LOOP done -- Let's roll!\n");
            state = SYS_state::LOOP;
            break;

    case SYS_state::LOOP:       
              
#ifdef HOCKEY  
            hockey.loop();
#endif     
            
#ifdef BOAT  
            boat.loop();
#endif              

            static unsigned long startTime1 = 0;
            static unsigned long startTime5 = 0;

            if (millis() - startTime1 >= 1000UL)
            {
                startTime1 = millis();  

                if(oledConnected) printOled();   

                if(Mqtt::isEnabled) sendHeartbeat();  
            }

            if (millis() - startTime5 >= 5000UL)
            {
                startTime5 = millis(); 

                if(bmxConnected) BMX280::actualizeWeather();  
#ifdef BOAT  
                boat.pressure = BMX280::pressure;
#endif                
                
                //Lux::loop(); 
                
               if(Mqtt::isEnabled) sendData();     //   TODO :   set les frequence d'envoi via la page web et config.
            }
            break;  

    }

}