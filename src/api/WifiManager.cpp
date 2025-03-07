#include "WifiManager.h"

#include <ArduinoOTA.h> 
#include <SPIFFS.h>

//  A data/config.txt file can be created, holding lines of ssid:password to preset a list of prefered network.
//  Apps will try to connect on user set network first, and then switch to prefered network list if previously unsuccessful
//  setup() and loop() must be called to use


void WifiManager::setup(bool enableOTA, String ssid, String password) 
{    
    WiFi.mode(WIFI_STA); // Start ESP32 in Station mode (client mode) 
    
    ssid_ = ssid;
    pass_ = password;
  
    bool check = tryConnectToUserNetwork(ssid, password);
    if(!check) tryConnectToPreferredNetworks(); 
    

    // If all preferred networks failed, start an Access Point
    if (!isWifiConnected)  startAccessPoint();

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
    //ArduinoOTA.setHostname("your_ota_hostname");

    isOTA = true;

   
    relaunchOTA(); // Start OTA
}

void WifiManager::relaunchOTA() 
{  
    ArduinoOTA.begin(); // Start OTA
    Serial.println("OTA Initialized");
}

void WifiManager::loop() 
{
    if ((!isWifiConnected) && isAutoReconnect) {
        if(!tryConnectToUserNetwork(ssid_, pass_)) tryConnectToPreferredNetworks(); 
        //tryConnectToUserNetwork(ssid_, pass_);
    }
    if(isOTA) handleOTA(); // Handle OTA updates
}

void WifiManager::handleOTA() 
{
  ArduinoOTA.handle();  // Handle OTA updates
}



bool WifiManager::tryConnectToUserNetwork(String ssid, String password) 
{
    Serial.print("Attempting to connect to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    int timeout = 15; // Wait for connection for 15 seconds
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    timeout--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        isWifiConnected = true;
      
        ipAdress = WiFi.localIP();

        Serial.print("Connected to ");  Serial.println(ssid);
        Serial.print("IP address: ");   Serial.println(ipAdress);
        //String ipMask = WiFi.subnetMask().toString();
        return true;
    }
 
    Serial.println("Failed to connect User network, trying preferred list");
    return false;
}



// Deprecated - Not used in the current implementation    tryConnectToUserNetwork is used instead

bool WifiManager::tryConnectToPreferredNetworks() 
{
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return false;
    }

    File configFile = SPIFFS.open("/config.txt", "r"); // Open the config file for reading   //  TODO : mettre en array dans esp32config.json

    if (!configFile) {    Serial.println("Failed to open config file");    return false;  }


        Serial.println("SPIFFS mounted and config.txt found for SSID and credentials");

    int i = 0;
    while (configFile.available() && i < numPreferredNetworks) {
        String line = configFile.readStringUntil('\n');
        line.trim();
        int separatorIndex = line.indexOf(':');

        if (separatorIndex != -1) {
        preferredSsids[i] = line.substring(0, separatorIndex).c_str();
        preferredPasswords[i] = line.substring(separatorIndex + 1).c_str();
        i++;
        }
    }
    configFile.close();

    for (int i = 0; i < numPreferredNetworks; i++) {
            const char* ssid = preferredSsids[i].c_str();
            const char* password = preferredPasswords[i].c_str();

            Serial.print("Attempting to connect to ");
            Serial.println(ssid);

            WiFi.begin(ssid, password);

            int timeout = 15; // Wait for connection for 15 seconds
            while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(1000);
            timeout--;
            }

            if (WiFi.status() == WL_CONNECTED) {
                isWifiConnected = true;
                ssid_ = (char*)ssid;
                ipAdress = WiFi.localIP();

                Serial.print("Connected to ");  Serial.println(ssid);
                Serial.print("IP address: ");   Serial.println(ipAdress);
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
    ipAdress = WiFi.softAPIP();
    Serial.print(F("Access Point IP address: ")); Serial.println(ipAdress);
  // Additional configurations for the Access Point can be done here
}

bool WifiManager::isConnected() 
{
  return isWifiConnected;
}

String WifiManager::getSSID() {     return ssid_;    }

void WifiManager::setSSID(String ssid) {     ssid_ =  ssid;  }

//char* WifiManager::getPASS() {     return pass_;    }

void WifiManager::setPASS(String pass) {     pass_ =  pass;  }




int WifiManager::getWiFiStrength(int points)   
{
    long rssi = 0;
    long avg_rssi = 0;
//  TODO :  si on passe le Alarm.delay(20) en calback a la fonction a la place, on deviendrait indépendant de la solution de temps time/alarm
    for (int i = 0; i < points; i++){
        rssi += WiFi.RSSI();
        delay(20);  
    }

    avg_rssi = rssi / points;
    return avg_rssi;
}

IPAddress WifiManager::getIP() 
{
     return ipAdress; 
}

String WifiManager::getIPString()
{  
  String s="";
  IPAddress ip = WifiManager::getIP();
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}