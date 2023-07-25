#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoOTA.h> // Include the ArduinoOTA library
#include <string> // Include the <string> header for std::string

class WifiManager {
public:
  void begin();
  bool isConnected();
  void loop();
  IPAddress getIP();

private:
  void startAccessPoint();
  void connectToWifi();
  bool tryConnectToPreferredNetworks();

  void setupOTA();
  void handleOTA();

  bool isWifiConnected = false;
  const static int numPreferredNetworks = 3; // Update this based on your preferred SSID count
  std::string preferredSsids[numPreferredNetworks];
  std::string preferredPasswords[numPreferredNetworks];

  IPAddress ipAdress;
};

#endif // WIFI_MANAGER_H