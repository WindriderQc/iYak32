#pragma once

#include <WiFi.h>         // For WiFiClient
#include <PubSubClient.h> // For PubSubClient
#include <ArduinoJson.h>  // For JsonDocument (used in sendJson signature if it were passed, but not directly needed for declarations here)
#include <list>           // For std::list
#include <vector>         // For std::vector
#include <SPIFFS.h>       // For File (used in getCredentials, though not in its signature directly)

// Forward declaration (if needed, but PubSubClient is included fully)
// class PubSubClient;
// class WiFiClient;

namespace Mqtt 
{
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
    const int RECONNECT_TIMEOUT = 6;
    const int MAX_MQTT_QUEUE_SIZE = 20;
 
    // Struct Definitions (remain in .h)
    struct MqttMsg 
    {
        char topicChar[128]; 
        String topicStr;
        char *topicTokens[MAX_TOKEN];
        char msgArray[50];  
        String msgStr;
        char *msgTokens[MAX_TOKEN];   
    };

    struct MqttQueue 
    {
        std::list<String> topics;
        std::list<String> messages;

        void add(String topic, String msg); // Declaration
        void publish();                     // Declaration
    };

    extern MqttQueue mqttQueue; // Instance declaration

    // Function Declarations
    MqttMsg sliceMqttMsg(char* topic, byte* message, unsigned int length);
    void subscription(String deviceName); // Corrected spelling
    bool getCredentials(bool isSpiffsMounted);
    bool setup(String deviceName, String mqttIP, bool isSpiffsMounted, int server_port_param = 1883);
    void loop();
    void sendJson(std::vector<String> names, std::vector<String> values, String topic, bool print2console = false); // Old function

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

} // namespace Mqtt
