#include "WifiManager.h"
#include "Esp32.h" // Added to access Esp32::spiffsMounted

#include <ArduinoOTA.h> 
#include <SPIFFS.h>
#include <ArduinoJson.h>

//  A data/config.txt file can be created, holding lines of ssid:password to preset a list of prefered network.
//  Apps will try to connect on user set network first, and then switch to prefered network list if previously unsuccessful
//  setup() and loop() must be called to use


void WifiManager::setup(bool enableOTA, String ssid, String password) 
{    
    WiFi.mode(WIFI_STA); // Start ESP32 in Station mode (client mode) 
    WiFi.disconnect();
    isWifiConnected_ = false;
    isAutoReconnect_ = true;
    
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

    ArduinoOTA.onStart([this]() {
        isOTAInProgress_ = true;
        Serial.println(F("OTA start"));
    });
    ArduinoOTA.onEnd([this]() {
        isOTAInProgress_ = false;
        Serial.println(F("OTA end"));
    });
    ArduinoOTA.onError([this](ota_error_t error) {
        isOTAInProgress_ = false;
        Serial.printf("OTA error[%u]\n", error);
    });

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
    if(isOTA_) handleOTA(); // Handle OTA updates first

    if (isOTAInProgress_) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        isWifiConnected_ = false;
    }

    if ((!isWifiConnected_) && isAutoReconnect_) {
        if (reconnect_state_ == ReconnectState::IDLE) {
            if (last_reconnect_attempt_ms_ == 0 || (millis() - last_reconnect_attempt_ms_) >= reconnect_interval_ms_) {
                last_reconnect_attempt_ms_ = millis();
                // Start non-blocking reconnect attempt
                if (!ssid_.isEmpty() && ssid_ != "iYak32") {
                    WiFi.begin(ssid_, password_);
                    reconnect_start_time_ = millis();
                    reconnect_state_ = ReconnectState::CONNECTING_USER;
                    Serial.print(F("WiFi: Reconnecting to "));
                    Serial.println(ssid_);
                } else {
                    preferred_network_index_ = 0;
                    reconnect_state_ = ReconnectState::CONNECTING_PREFERRED;
                }
            }
        } else {
            tryReconnectStep();
        }
    }
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
    ssid.trim();
    password.trim();

    Serial.print("Attempting to connect to User network: ");
    if(ssid.isEmpty()) {
        Serial.println(" - No user network provided");
        return false;
    }

    if (ssid == "iYak32") {
        Serial.println("iYak32 (AP fallback SSID detected, skipping as user network)");
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

bool WifiManager::tryConnectToPreferredNetworks() 
{
    if (!Esp32::spiffsMounted) {
        Serial.println(F("WifiManager Error: SPIFFS not mounted. Cannot load preferred networks from config.txt."));
        return false;
    }

    File configFile = SPIFFS.open("/config.txt", "r");
    if (!configFile) {
        Serial.println("Failed to open config.txt");
        return false;
    }

    Serial.println("SPIFFS mounted and config.txt found for SSID and credentials");

    while (configFile.available()) {
        String line = configFile.readStringUntil('\n');
        line.trim();

        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        int separatorPos = line.indexOf(':');
        if (separatorPos <= 0) {
            continue;
        }

        String candidateSsid = line.substring(0, separatorPos);
        String candidatePassword = line.substring(separatorPos + 1);

        candidateSsid.trim();
        candidatePassword.trim();

        if (candidateSsid.isEmpty()) {
            continue;
        }

        Serial.print("Attempting to connect to Preferred network: ");
        Serial.println(candidateSsid);

        WiFi.begin(candidateSsid.c_str(), candidatePassword.c_str());

        int timeout = 15;
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(1000);
            timeout--;
        }

        if (WiFi.status() == WL_CONNECTED) {
            isWifiConnected_ = true;
            ssid_ = candidateSsid;
            ipAddress_ = WiFi.localIP();

            Serial.print("Connected to ");  Serial.println(candidateSsid);
            Serial.print("IP address: ");   Serial.println(ipAddress_);
            configFile.close();
            return true;
        }
    }

    configFile.close();
    Serial.println("Failed to connect to any preferred network from config.txt");
    return false;
}


void WifiManager::loadPreferredNetworks() {
    if (preferred_networks_loaded_) return;
    preferredSsids_.clear();
    preferredPasswords_.clear();

    if (!Esp32::spiffsMounted) return;

    File configFile = SPIFFS.open("/config.txt", "r");
    if (!configFile) return;

    while (configFile.available()) {
        String line = configFile.readStringUntil('\n');
        line.trim();
        if (line.isEmpty() || line.startsWith("#")) continue;
        int sep = line.indexOf(':');
        if (sep <= 0) continue;
        String s = line.substring(0, sep);
        String p = line.substring(sep + 1);
        s.trim(); p.trim();
        if (!s.isEmpty()) {
            preferredSsids_.push_back(s);
            preferredPasswords_.push_back(p);
        }
    }
    configFile.close();
    preferred_networks_loaded_ = true;
}

bool WifiManager::tryReconnectStep() {
    const unsigned long CONNECT_TIMEOUT_MS = 15000;

    switch (reconnect_state_) {
        case ReconnectState::CONNECTING_USER: {
            if (WiFi.status() == WL_CONNECTED) {
                isWifiConnected_ = true;
                ipAddress_ = WiFi.localIP();
                reconnect_state_ = ReconnectState::IDLE;
                Serial.print(F("WiFi: Reconnected to ")); Serial.println(ssid_);
                return true;
            }
            if (millis() - reconnect_start_time_ > CONNECT_TIMEOUT_MS) {
                Serial.println(F("WiFi: User network reconnect timeout, trying preferred..."));
                preferred_network_index_ = 0;
                reconnect_start_time_ = 0;  // Reset so CONNECTING_PREFERRED doesn't inherit stale elapsed time
                reconnect_state_ = ReconnectState::CONNECTING_PREFERRED;
            }
            return false;
        }
        case ReconnectState::CONNECTING_PREFERRED: {
            // Check if current attempt succeeded
            if (WiFi.status() == WL_CONNECTED) {
                isWifiConnected_ = true;
                ipAddress_ = WiFi.localIP();
                if (preferred_network_index_ > 0 && preferred_network_index_ <= (int)preferredSsids_.size()) {
                    ssid_ = preferredSsids_[preferred_network_index_ - 1];
                }
                reconnect_state_ = ReconnectState::IDLE;
                Serial.print(F("WiFi: Reconnected. IP: ")); Serial.println(ipAddress_);
                return true;
            }

            loadPreferredNetworks();

            // Check timeout for current preferred network attempt
            if (reconnect_start_time_ != 0 && (millis() - reconnect_start_time_) < CONNECT_TIMEOUT_MS) {
                return false; // Still waiting
            }

            // Move to next preferred network
            if (preferred_network_index_ < (int)preferredSsids_.size()) {
                Serial.print(F("WiFi: Trying preferred network: "));
                Serial.println(preferredSsids_[preferred_network_index_]);
                WiFi.begin(preferredSsids_[preferred_network_index_].c_str(),
                           preferredPasswords_[preferred_network_index_].c_str());
                reconnect_start_time_ = millis();
                preferred_network_index_++;
                return false;
            }

            // Exhausted all networks
            Serial.println(F("WiFi: All reconnect attempts failed."));
            reconnect_state_ = ReconnectState::IDLE;
            return false;
        }
        default:
            reconnect_state_ = ReconnectState::IDLE;
            return false;
    }
}

void WifiManager::startAccessPoint()
{
    const char* apSsid = "iYak32";

    // Use configurable AP password if set, otherwise derive from device MAC
    String apPasswordStr = Esp32::configJson_["ap_password"] | "";
    apPasswordStr.trim();
    if (apPasswordStr.isEmpty()) {
        // Derive per-device password from MAC-based device name (e.g. "iYak_A1B2_32")
        apPasswordStr = "iYak_" + Esp32::DEVICE_NAME.substring(4) + "_32";
    }

    WiFi.softAP(apSsid, apPasswordStr.c_str());

    ssid_ = (char*)apSsid;
    ipAddress_ = WiFi.softAPIP();
    Serial.print(F("Access Point started. SSID: ")); Serial.println(apSsid);
    Serial.print(F("AP Password: ")); Serial.println(apPasswordStr);
    Serial.print(F("AP IP address: ")); Serial.println(ipAddress_);
}

bool WifiManager::isConnected() 
{
  return isWifiConnected_;
}

String WifiManager::getSSID() {     return ssid_;    }

void WifiManager::setSSID(String ssid) {     ssid_ =  ssid;  }

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