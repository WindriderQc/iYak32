#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoOTA.h> // Include the ArduinoOTA library
#include <string> // Include the <string> header for std::string

/*
Auto connect to wifi using a list of preferred network in SPIFF /config.txt containing lines of "SSID:PASSWORD"  
Access point will be created if wifi connection fail.    ssid = "iYak32"  apPassword = "12345678"
OTA esp32 reprogramming setup and handle; IP address must be set in platformio.ini
esp32 time is configured upon NTP server

TODO: possibility to have a fixed IP address configured from EEPROM?  and adapt time configuration to handle accessPoint mode
*/

class WifiManager {
    public:
        void setup();
        bool isConnected();
        void loop();
        IPAddress getIP();
        String getIPString();
        char* getSSID();
        int getWiFiStrength(int points = 10); 

    private:
        void setupOTA();
        void handleOTA();
        bool tryConnectToPreferredNetworks();
        void startAccessPoint();
    
        bool isWifiConnected = false;
        const static int numPreferredNetworks = 3; // Update this based on your preferred SSID count  TODO:  ark.... lire le nbr de ligne dans config.txt? ou whatever?
        std::string preferredSsids[numPreferredNetworks];
        std::string preferredPasswords[numPreferredNetworks];

        bool isAutoReconnect = false;

        IPAddress ipAdress;
        char* ssid_;
};

#endif // WIFI_MANAGER_H