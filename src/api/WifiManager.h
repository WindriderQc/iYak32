#pragma once

#include <string> // Include the <string> header for std::string
#include <WiFi.h>



/*
Auto connect to wifi using a list of preferred network in SPIFF /config.txt containing lines of "SSID:PASSWORD"  
Access point will be created if wifi connection fail.    ssid = "iYak32"  apPassword = "12345678"
OTA esp32 reprogramming setup and handle; IP address must be set in platformio.ini
esp32 time is configured upon NTP server

TODO: possibility to have a fixed IP address configured from EEPROM?  and adapt time configuration to handle accessPoint mode
*/

class WifiManager {
    public:
        void setup(bool enableOTA, String ssid, String password);
        void setup(bool enableOTA);
        void loop();

        bool isConnected();
        IPAddress getIP();
        String getIPString();
        String getSSID();
        void setSSID(String ssid);
        String getPASS();
        void setPASS(String pass);
        int getWiFiStrength(int points = 10); 
        void relaunchOTA();

    private:
        void setupOTA();
        void handleOTA();
        bool tryConnectToPreferredNetworks();
        bool tryConnectToUserNetwork(String ssid, String password);
        void startAccessPoint();
    
        bool isWifiConnected = false;
        const static int numPreferredNetworks = 3; // Update this based on your preferred SSID count  TODO:  ark.... lire le nbr de ligne dans config.txt? ou whatever?
        String preferredSsids[numPreferredNetworks];
        String preferredPasswords[numPreferredNetworks];

        bool isAutoReconnect = false;
        bool isOTA = false;
   
        IPAddress ipAdress;
        String ssid_;
        String pass_;
};