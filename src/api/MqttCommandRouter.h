#pragma once

#include <Arduino.h>

namespace MqttCommandRouter {
    void handleIncoming(char* topic, byte* message, unsigned int length);
}
