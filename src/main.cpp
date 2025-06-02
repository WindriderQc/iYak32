#define VERBOSE
//#define HOCKEY
//#define BOAT
#define BASIC_MODE

#include <Arduino.h>
#include "SystemState.h" // Added for SYS_state enum
#include "api/Esp32.h"
#include "api/Mqtt.h"    // Added for direct Mqtt namespace usage
#include "api/devices/BMX280.h"  //  TODO : devrait etre encaps dans un device...   device = gestion IO = main config n feature focus
#include "api/devices/Oled.h"

#include "www.h"

#ifdef BOAT
    #include "Boat.h"
#endif

#ifdef HOCKEY  
    #include "Hockey.h"
#endif

#ifdef BASIC_MODE
    #include "BasicMode.h"
#endif

const char* ver = "v1:7 ";




bool oledConnected = false;
bool bmxConnected = false;

//  STATE Machine - System 
// enum class SYS_state { // Moved to SystemState.h
//     BOOT,
//     DEVICES_CONFIG,
//     HEATUP,
//     FIRSTLOOP,
//     LOOP
// };
static SYS_state state = SYS_state::BOOT; // Reverted to original name

// Accessor functions for the static 'state' variable
SYS_state get_current_system_state() { // Reverted to original name
    return state;
}

void set_current_system_state(SYS_state new_state) { // Reverted to original name
    // Optional: Log state changes centrally
    // if (state != new_state) {
    //    Serial.printf("System State: %d -> %d (via setter)\n", static_cast<int>(state), static_cast<int>(new_state));
    // }
    state = new_state;
}


void sendHeartbeat() {
    #ifdef VERBOSE
        bool printToConsole = true;
    #else
        bool printToConsole = false;
    #endif

    JsonDocument payload_doc;
    // payload_doc["status"] = "online"; // Example, or keep empty
    JsonObject payload_obj = payload_doc.to<JsonObject>();

    Mqtt::publishStandardMessage(
        "esp32/alive/" + Esp32::DEVICE_NAME, // topic
        "heartbeat",                          // message_type
        payload_obj,                          // payload
        "info",                               // status
        "",                                   // command_id
        payload_obj.isNull() || payload_obj.size() == 0 ? "" : "simple_status", // payload_type
        printToConsole
    );
}

void sendData() {
    #ifdef VERBOSE
        bool printToConsole = true;
    #else
        bool printToConsole = false;
    #endif

    JsonDocument payload_doc;

    payload_doc["cpu_temp_c"] = Esp32::getCPUTemp();
    payload_doc["wifi_rssi"] = Esp32::wifiManager.getWiFiStrength();
    payload_doc["battery_v"] = Esp32::getBattRemaining(false);

    if (bmxConnected) {
        payload_doc["bmx_temp_c"] = BMX280::getTemperature();
        payload_doc["bmx_pressure_hpa"] = BMX280::getPressure();
        payload_doc["bmx_altitude_m"] = BMX280::getAltitude();
        // USE_BMPvsBME is handled by BMX280::getHumidity() which returns 0.0 for BMP
        payload_doc["bmx_humidity_pct"] = BMX280::getHumidity();
    }
    // Add other sensor data here if any
    /*names.push_back("co2");         values.push_back(String(Weather::co2));
    names.push_back("smoke");       values.push_back(String(Weather::smoke));
    names.push_back("lpg");         values.push_back(String(Weather::lpg));
    names.push_back("airHumid");    values.push_back(String(Weather::airHumidity));
    names.push_back("tempDht");     values.push_back(String(Weather::tempDht));
    names.push_back("ir");          values.push_back(String(Lux::ir_));
    names.push_back("full");        values.push_back(String(Lux::full_));
    names.push_back("visible");     values.push_back(String(Lux::visible_));
    names.push_back("lux");         values.push_back(String(Lux::lux_));*/

    JsonObject payload_obj = payload_doc.as<JsonObject>();

    Mqtt::publishStandardMessage(
        "esp32/data/" + Esp32::DEVICE_NAME,  // topic
        "sensor_data",                       // message_type
        payload_obj,                         // payload
        "info",                              // status
        "",                                  // command_id
        "sensor_readings",                   // payload_type
        printToConsole
    );
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
    Oled::oled.printf("\nKPa: %.2f", BMX280::getPressure());
    Oled::oled.printf("\ntC: %.1f", BMX280::getTemperature());
    Oled::oled.printf("\nAlt: %.1f", BMX280::getAltitude());
    // Oled::oled.printf("\nHum: %.1f", BMX280::getHumidity()); // Example

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
    

    switch(state) // Reverted to original name
    {
    case SYS_state::BOOT:
            
            if(Mqtt::isEnabled)             Mqtt::mqttClient.publish("esp32/register", Esp32::DEVICE_NAME.c_str() ); //Once connected, publish an announcement...    
            if(Esp32::isConfigFromServer)   Mqtt::mqttClient.publish("esp32/config", Esp32::DEVICE_NAME.c_str());  // Request IO config and profile from server
            
            Serial.println("BOOT done");
            set_current_system_state(SYS_state::DEVICES_CONFIG); // Reverted to original setter
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

#ifdef BASIC_MODE
        BasicMode::basicMode.setup();
#endif

            Serial.println("SENSORS & ALARMLib CONFIG done");
            set_current_system_state(SYS_state::HEATUP); // Reverted to original setter
            break;
    
    case SYS_state::HEATUP:

            Serial.print("Heating up...");
           // insure all sensor are heated up to send accurate data at start
            if(millis() <= 3000) {
                Serial.print("."); Serial.print(millis()); Serial.print("."); 
            }  
            else {
                set_current_system_state(SYS_state::FIRSTLOOP); // Reverted to original setter
                Serial.println(" On Fire! :)");
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
            set_current_system_state(SYS_state::LOOP); // Reverted to original setter
            break;

    case SYS_state::LOOP:       
              
#ifdef HOCKEY  
            hockey.loop();
#endif     

#ifdef BASIC_MODE
        BasicMode::basicMode.loop();
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

            if (millis() - startTime5 >= (static_cast<unsigned long>(Esp32::mqtt_data_interval_seconds_) * 1000UL))
            {
                startTime5 = millis(); 

                if(bmxConnected) BMX280::actualizeWeather();  
#ifdef BOAT  
                Boat::boat.pressure = BMX280::getPressure(); // Assuming Boat::boat is the global instance
#endif                
                
                //Lux::loop(); 
                
               if(Mqtt::isEnabled) sendData();     //   TODO :   set les frequence d'envoi via la page web et config.
            }
            break;  

    }

}