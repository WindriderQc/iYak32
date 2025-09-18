#pragma once

#include <EEPROM.h>
#include "SPIFFS.h"

namespace Storage {
    extern int startingAddress;

    void IPtoEEPROM(IPAddress IPaddr);
    IPAddress readIPFromEEPROM();

    void listDir(const char * dirname, uint8_t levels);
    void removeConfigFile(String config_filename);
    void dumpFile(String config_filename, int max_char_dump = 1024);
    String readFile(String filename);
    bool writeFile(String filename, String message);
}