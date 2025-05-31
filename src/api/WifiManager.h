#pragma once

#include <string> // Include the <string> header for std::string
#include <vector> // Added for std::vector
#include <WiFi.h>
#include <ESPmDNS.h>



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
    
        bool isWifiConnected_ = false;
        // const static int numPreferredNetworks = 3; // Removed
        std::vector<String> preferredSsids_; // Changed to std::vector<String>
        std::vector<String> preferredPasswords_; // Changed to std::vector<String>

        bool isAutoReconnect_ = false;
        bool isOTA_ = false;
   
        IPAddress ipAddress_;
        String ssid_; // Stays as is (already has underscore, though not strictly private style, but per instructions)
        String password_; // Renamed from pass_
};