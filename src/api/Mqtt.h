#pragma once

#include <WiFi.h>         // For WiFiClient
#include <PubSubClient.h> // For PubSubClient
#include <ArduinoJson.h>  // For JsonDocument (used in sendJson signature if it were passed, but not directly needed for declarations here)
#include <list>           // For std::list
#include <vector>         // For std::vector

// Forward declaration (if needed, but PubSubClient is included fully)
// class PubSubClient;
// class WiFiClient;

namespace Mqtt
{
    // MQTT Connection State Machine
    enum class MqttState {
        OFF,            // MQTT not enabled in config
        DISCONNECTED,   // Idle, waiting for retry timer
        CONNECTING,     // Actively attempting connect (max 3 tries per burst)
        CONNECTED,      // Normal operation
        BACKOFF         // Exponential wait before next attempt
    };

    // Extern Namespace Variables (declarations)
    extern bool isEnabled;
    extern String serverIp;
    extern int port;
    extern String mqttUser;
    extern String mqttPass;
    extern WiFiClient client;       // Declaration
    extern PubSubClient mqttClient; // Declaration

    // Constants (remain defined in .h)
    const int MAX_TOKEN = 10;
    const int MAX_ATTEMPTS_PER_BURST = 3;
    const int MAX_MQTT_QUEUE_SIZE = 20;
    const unsigned long BACKOFF_BASE_MS = 5000;      // 5s initial backoff
    const unsigned long BACKOFF_MAX_MS = 60000;       // 60s max backoff cap

    // Struct Definitions (remain in .h)
    struct MqttMsg
    {
        char topicChar[128];
        String topicStr;
        char *topicTokens[MAX_TOKEN];
        char msgArray[256];
        String msgStr;
        char *msgTokens[MAX_TOKEN];
        bool topicTruncated = false;
        bool msgTruncated = false;
    };

    struct MqttQueue
    {
        std::list<String> topics;
        std::list<String> messages;

        void add(String topic, String msg); // Declaration
        void publish();                     // Declaration
    };

    extern MqttQueue mqttQueue; // Instance declaration

    // Connection state (non-blocking reconnect)
    extern MqttState currentState_;
    extern unsigned long lastConnectAttempt_;
    extern int connectAttemptCount_;
    extern int backoffMultiplier_;
    extern unsigned long backoffDuration_;
    extern String pendingDeviceName_;

    // Function Declarations
    MqttMsg sliceMqttMsg(char* topic, byte* message, unsigned int length);
    void subscription(String deviceName);
    bool setup(String deviceName, const JsonDocument& config);
    void loop();
    void sendJson(std::vector<String> names, std::vector<String> values, String topic, bool print2console = false);

    // New standardized message publishing function
    void publishStandardMessage(
        const String& topic,
        const String& message_type,
        const JsonObjectConst& payload,
        const String& status = "",
        const String& command_id = "",
        const String& payload_type = "",
        bool printToConsole = false
    );

    // State machine API
    MqttState getState();
    const char* getStateString();
    void forceReconnect();

} // namespace Mqtt
