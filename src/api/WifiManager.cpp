#include "WifiManager.h"
#include "Esp32.h" // Added to access Esp32::spiffsMounted

#include <ArduinoOTA.h> 
#include <SPIFFS.h>

//  A data/config.txt file can be created, holding lines of ssid:password to preset a list of prefered network.
//  Apps will try to connect on user set network first, and then switch to prefered network list if previously unsuccessful
//  setup() and loop() must be called to use


void WifiManager::setup(bool enableOTA, String ssid, String password) 
{    
    WiFi.mode(WIFI_STA); // Start ESP32 in Station mode (client mode) 
    
    ssid_ = ssid;
    password_ = password;
  
    bool check = tryConnectToUserNetwork(ssid, password);
    if(!check) tryConnectToPreferredNetworks(); 
    

    // If all preferred networks failed, start an Access Point
    if (!isWifiConnected_)  startAccessPoint();

    if(enableOTA) setupOTA(); // Initialize OTA

    /*//#define FIXED_IP
    #ifdef FIXED_IP
    // Set your Static IP address
        IPAddress local_IP(192, 168, 0, 200);
        // Set your Gateway IP address
        IPAddress gateway(192, 168, 0, 1);

        IPAddress subnet(255, 255, 255, 0);
        // IPAddress primaryDNS(8, 8, 8, 8); // optional
        // IPAddress secondaryDNS(8, 8, 4, 4); // optional
    #else 
        IPAddress local_IP(0,0,0,0);
    #endif*/
}

void WifiManager::setupOTA() 
{
    // Authenticate OTA updates with a password (optional)
    // Remove the line below if you don't want authentication
    //ArduinoOTA.setPassword("your_ota_password");

    // Initialize OTA with a hostname (optional)
    // By default, the hostname will be "esp32-[MAC address]"
    const char* hostname = "iyak32"; // Define your desired hostname
    ArduinoOTA.setHostname(hostname);

    isOTA_ = true;

   
    relaunchOTA(); // Start OTA

    // Start mDNS for OTA discovery if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        if (MDNS.begin(hostname)) { // Pass only hostname
            MDNS.addService("arduino", "tcp", 3232); // 3232 is default ESP32 OTA port. "arduino" is service type for PlatformIO
            Serial.printf("mDNS responder started for OTA: http://%s.local. OTA on port 3232\n", hostname);
        } else {
            Serial.println(F("Error setting up MDNS responder for OTA!"));
        }
    } else {
        Serial.println(F("WiFi not connected, MDNS for OTA not started."));
    }
}

void WifiManager::relaunchOTA() 
{  
    ArduinoOTA.begin(); // Start OTA
    Serial.println("OTA Initialized");
}

void WifiManager::loop() 
{
    if ((!isWifiConnected_) && isAutoReconnect_) {
        if(!tryConnectToUserNetwork(ssid_, password_)) tryConnectToPreferredNetworks();
        //tryConnectToUserNetwork(ssid_, password_);
    }
    if(isOTA_) handleOTA(); // Handle OTA updates

    // Non-blocking WiFi Strength Sampling & Averaging
    const unsigned int RSSI_SAMPLE_INTERVAL_MS = 200; // How often to take a new sample

    if (isWifiConnected_ && WiFi.status() == WL_CONNECTED) {
        if (millis() - last_rssi_sample_time_ >= RSSI_SAMPLE_INTERVAL_MS || last_rssi_sample_time_ == 0) {
            last_rssi_sample_time_ = millis();
            long current_rssi = WiFi.RSSI();

            if (rssi_samples_.size() >= max_rssi_samples_) {
                rssi_samples_.erase(rssi_samples_.begin()); // Remove the oldest sample
            }
            rssi_samples_.push_back(current_rssi);

            if (!rssi_samples_.empty()) {
                long sum_rssi = 0;
                for (long sample : rssi_samples_) {
                    sum_rssi += sample;
                }
                current_avg_rssi_ = sum_rssi / rssi_samples_.size();
            } else {
                current_avg_rssi_ = current_rssi;
            }
        }
    } else if (isWifiConnected_ && WiFi.status() != WL_CONNECTED) {
        // Was connected, now lost connection
        rssi_samples_.clear();
        current_avg_rssi_ = 0;
    } else if (!isWifiConnected_ && !rssi_samples_.empty()) {
        // Explicitly not connected, clear samples
        rssi_samples_.clear();
        current_avg_rssi_ = 0;
    }
}

void WifiManager::handleOTA() 
{
  ArduinoOTA.handle();  // Handle OTA updates
}



bool WifiManager::tryConnectToUserNetwork(String ssid, String password) 
{
    Serial.print("Attempting to connect to User network");
    if(ssid.isEmpty()) {
        Serial.println(" - No user network provided");
        return false;
    }
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    int timeout = 15; // Wait for connection for 15 seconds
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    timeout--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        isWifiConnected_ = true;
      
        ipAddress_ = WiFi.localIP();

        Serial.print("Connected to ");  Serial.println(ssid);
        Serial.print("IP address: ");   Serial.println(ipAddress_);
        //String ipMask = WiFi.subnetMask().toString();
        return true;
    }
 
    Serial.println("Failed to connect User network, trying preferred list");
    return false;
}



// Deprecated - Not used in the current implementation    tryConnectToUserNetwork is used instead

bool WifiManager::tryConnectToPreferredNetworks() 
{
    if (!Esp32::spiffsMounted) {
        Serial.println(F("WifiManager Error: SPIFFS not mounted. Cannot load preferred networks from config.txt."));
        return false;
    }

    File configFile = SPIFFS.open("/config.txt", "r"); // Open the config file for reading   //  TODO : mettre en array dans esp32config.json

    if (!configFile) {    Serial.println("Failed to open config file");    return false;  }

    Serial.println("SPIFFS mounted and config.txt found for SSID and credentials. Reading all entries.");

    preferredSsids_.clear();
    preferredPasswords_.clear();

    while (configFile.available()) {
        String line = configFile.readStringUntil('\n');
        line.trim();
        int separatorIndex = line.indexOf(':');

        if (separatorIndex != -1) {
            preferredSsids_.push_back(line.substring(0, separatorIndex));
            preferredPasswords_.push_back(line.substring(separatorIndex + 1));
        }
    }
    configFile.close();

    for (size_t i = 0; i < preferredSsids_.size(); i++) { // Iterate using vector's size
            const char* ssid = preferredSsids_[i].c_str();
            const char* password = preferredPasswords_[i].c_str();

            Serial.print("Attempting to connect to Preferred network: ");
            Serial.println(ssid);

            WiFi.begin(ssid, password);

            int timeout = 15; // Wait for connection for 15 seconds
            while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(1000);
            timeout--;
            }

            if (WiFi.status() == WL_CONNECTED) {
                isWifiConnected_ = true;
                ssid_ = (char*)ssid;
                ipAddress_ = WiFi.localIP();

                Serial.print("Connected to ");  Serial.println(ssid);
                Serial.print("IP address: ");   Serial.println(ipAddress_);
                //String ipMask = WiFi.subnetMask().toString();
                    
            
                return true;
            }
    }

    Serial.println("Failed to connect to any preferred network");
    return false;
}


void WifiManager::startAccessPoint() 
{
    // Set the SSID (name) and password of the Access Point
    const char* apSsid = "iYak32";
    const char* apPassword = "12345678";

    // Start the Access Point with the specified SSID and password
    WiFi.softAP(apSsid, apPassword);

    ssid_ = (char*)apSsid;
    ipAddress_ = WiFi.softAPIP();
    Serial.print(F("Access Point IP address: ")); Serial.println(ipAddress_);
  // Additional configurations for the Access Point can be done here
}

bool WifiManager::isConnected() 
{
  return isWifiConnected_;
}

String WifiManager::getSSID() {     return ssid_;    }

void WifiManager::setSSID(String ssid) {     ssid_ =  ssid;  }

//char* WifiManager::getPASS() {     return password_;    } // Assuming getPASS should reflect new name

void WifiManager::setPASS(String pass) {     password_ =  pass;  }




int WifiManager::getWiFiStrength()
{
    if (!isWifiConnected_ || rssi_samples_.empty()) {
        return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0; // Return current instant RSSI or 0
    }
    return current_avg_rssi_;
}

void WifiManager::setRssiMaxSamples(byte samples) {
    if (samples > 0 && samples <= 50) {
        max_rssi_samples_ = samples;
        rssi_samples_.clear();
        current_avg_rssi_ = 0;
        last_rssi_sample_time_ = 0; // Force immediate new sample in loop
    }
}

IPAddress WifiManager::getIP() 
{
     return ipAddress_;
}

String WifiManager::getIPString()
{  
  String s="";
  IPAddress ip = WifiManager::getIP(); // This will now call getIP() which returns ipAddress_
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}