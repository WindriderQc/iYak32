#include "WifiManager.h"

void WifiManager::begin() {
  // Start ESP32 in Station mode (client mode)
  WiFi.mode(WIFI_STA);
  tryConnectToPreferredNetworks();

  // If all preferred networks failed, start an Access Point
  if (!isWifiConnected) {
    startAccessPoint();
  }

  setupOTA(); // Initialize OTA
}

bool WifiManager::isConnected() {
  return isWifiConnected;
}

void WifiManager::loop() {

  if ((!isWifiConnected) && isAutoReconnect)  tryConnectToPreferredNetworks(); 
 
  handleOTA(); // Handle OTA updates

}

IPAddress WifiManager::getIP() {
     return ipAdress; 
}

char* WifiManager::getSSID() {
     return ssid_; 
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
        Serial.print("Connected to ");
        Serial.println(ssid);
        ssid_ = (char*)ssid;
        Serial.print("IP address: ");
        ipAdress = WiFi.localIP();
        Serial.println(ipAdress);
        return true;
    }
  }

  Serial.println("Failed to connect to any preferred network");
  return false;
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
}

void WifiManager::handleOTA() {
  // Handle OTA updates
  ArduinoOTA.handle();
}
