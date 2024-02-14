#include "WifiManager.h"

void WifiManager::setup() {
  // Start ESP32 in Station mode (client mode)
  WiFi.mode(WIFI_STA);
  tryConnectToPreferredNetworks();

  // If all preferred networks failed, start an Access Point
  if (!isWifiConnected) {
    startAccessPoint();
  }

  setupOTA(); // Initialize OTA


/*

 //#define FIXED_IP
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
 #endif
 
 */
}

void WifiManager::setupOTA() {
  // Authenticate OTA updates with a password (optional)
  // Remove the line below if you don't want authentication
  //ArduinoOTA.setPassword("your_ota_password");

  // Initialize OTA with a hostname (optional)
  // By default, the hostname will be "esp32-[MAC address]"
  //ArduinoOTA.setHostname("your_ota_hostname");

  // Start OTA
  ArduinoOTA.begin();
  Serial.println("OTA Initialized");



     
 //   void setupOTA(const char* ssid = "SM-A520W7259", const char* password = "Alouette54321")
  //  void setupOTA(const char* ssid, const char* password)
    /*void setupOTA()
    {
        WiFi.mode(WIFI_STA);
       // WiFi.begin(ssid, password);
        WiFi.begin("UGLink", "Alouette54321!");
        
        uint32_t notConnectedCounter = 0;
        Serial.print("\nWifi connecting...");
        while (WiFi.status() != WL_CONNECTED) {
            delay(100);
            Serial.print("...");
            notConnectedCounter++;
            if(notConnectedCounter > 150) { // Reset board if not connected after 15s
                Serial.println("\nRebooting due to Wifi not connecting...");
                ESP.restart();
            }
        }

        // Port defaults to 3232
        // ArduinoOTA.setPort(3232);

        // Hostname defaults to esp3232-[MAC]
        // ArduinoOTA.setHostname("myesp32");

        // No authentication by default
        // ArduinoOTA.setPassword("admin");

        // Password can be set with it's md5 value as well
        // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
        // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

        ArduinoOTA
            .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
            })
            .onEnd([]() {
            Serial.println("\nEnd");
            })
            .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
            })
            .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");
            });

        ArduinoOTA.begin();

        Serial.println("Ready");
        Serial.print("IP address: ");
        Serial.println(getLocalIP());
    }*/


}

void WifiManager::loop() {

  if ((!isWifiConnected) && isAutoReconnect)  tryConnectToPreferredNetworks(); 
 
  handleOTA(); // Handle OTA updates

}

void WifiManager::handleOTA() {
  // Handle OTA updates
  ArduinoOTA.handle();
}

bool WifiManager::tryConnectToPreferredNetworks() {
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return false;
  }

  File configFile = SPIFFS.open("/config.txt", "r"); // Open the config file for reading

  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }


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

void WifiManager::startAccessPoint() {
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

bool WifiManager::isConnected() {
  return isWifiConnected;
}

char* WifiManager::getSSID() {
     return ssid_; 
}

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

IPAddress WifiManager::getIP() {
     return ipAdress; 
}

String WifiManager::getIPString(){  
  String s="";
  IPAddress ip = WifiManager::getIP();
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}

