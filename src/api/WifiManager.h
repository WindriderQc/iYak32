#pragma once

#include <string> // Include the <string> header for std::string
#include <vector> // Added for std::vector
#include <WiFi.h>
#include <ESPmDNS.h>



/*
Connect using the configured Wi-Fi network or optional SPIFFS /config.txt fallback
entries in SSID:PASSWORD format. Start a local access point if all attempts fail.
The AP password comes from ap_password or a per-device fallback.
OTA uploads require an explicit target address at upload time.
*/

class WifiManager {
    public:
        void setup(bool enableOTA, String ssid, String password);
        void loop();

        bool isConnected();
        IPAddress getIP();
        String getIPString();
        String getSSID();
        void setSSID(String ssid);
        void setPASS(String pass);
        int getWiFiStrength(); // Removed points parameter
        void relaunchOTA();
        void setRssiMaxSamples(byte samples); // Setter for max_rssi_samples_

    private:
        void setupOTA();
        void handleOTA();
        bool tryConnectToPreferredNetworks();
        bool tryConnectToUserNetwork(String ssid, String password);
        void startAccessPoint();
        void loadPreferredNetworks();
        bool tryReconnectStep();

        bool isWifiConnected_ = false;
        std::vector<String> preferredSsids_;
        std::vector<String> preferredPasswords_;

        bool isAutoReconnect_ = false;
        bool isOTA_ = false;
        bool isOTAInProgress_ = false;
        unsigned long last_reconnect_attempt_ms_ = 0;
        const unsigned long reconnect_interval_ms_ = 15000;

        IPAddress ipAddress_;
        String ssid_;
        String password_;

        // Non-blocking reconnect state machine
        enum class ReconnectState { IDLE, CONNECTING_USER, CONNECTING_PREFERRED };
        ReconnectState reconnect_state_ = ReconnectState::IDLE;
        unsigned long reconnect_start_time_ = 0;
        int preferred_network_index_ = 0;
        bool preferred_networks_loaded_ = false;

        // For non-blocking RSSI averaging
        std::vector<long> rssi_samples_;
        byte max_rssi_samples_ = 10;
        unsigned long last_rssi_sample_time_ = 0;
        long current_avg_rssi_ = 0;
};
