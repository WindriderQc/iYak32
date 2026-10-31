#include "Storage.h"
#include "ConfigFileStore.h"
#include <freertos/semphr.h>

namespace Storage {
    static SemaphoreHandle_t configMutex = xSemaphoreCreateMutex();
    struct ConfigFS {
        bool exists(const std::string& p) { return SPIFFS.exists(p.c_str()); }
        bool remove(const std::string& p) { return SPIFFS.remove(p.c_str()); }
        bool rename(const std::string& a, const std::string& b) { return SPIFFS.rename(a.c_str(), b.c_str()); }
        std::string read(const std::string& p) { File f = SPIFFS.open(p.c_str(), FILE_READ); if (!f) return {}; String content = f.readString(); f.close(); return std::string(content.c_str(), content.length()); }
        bool write(const std::string& p, const std::string& content) { File f = SPIFFS.open(p.c_str(), FILE_WRITE); if (!f) return false; size_t n = f.write(reinterpret_cast<const uint8_t*>(content.data()), content.size()); f.flush(); f.close(); return n == content.size(); }
    };
    String readConfigFile(const String& filename, ConfigFileStore::Root root) {
        xSemaphoreTake(configMutex, portMAX_DELAY); ConfigFS fs;
        std::string result = ConfigFileStore::read(fs, filename.c_str(), root);
        xSemaphoreGive(configMutex); return String(result.c_str());
    }
    bool writeConfigFile(const String& filename, const String& content, ConfigFileStore::Root root) {
        xSemaphoreTake(configMutex, portMAX_DELAY); ConfigFS fs;
        bool result = ConfigFileStore::save(fs, filename.c_str(), std::string(content.c_str(), content.length()), root);
        xSemaphoreGive(configMutex); return result;
    }

    int startingAddress = 0;

    void IPtoEEPROM(IPAddress IPaddr) {
        Serial.print("Saving IP to EEPROM : ");
        Serial.println(IPaddr);

        for (int n = 0; n < 4; n++) {
            EEPROM.write(startingAddress + n , IPaddr[n]);
            Serial.println(IPaddr[n]);
        }

        EEPROM.commit() ? Serial.println(F("\nEEPROM successfully committed")) : Serial.println(F("\nERROR! EEPROM commit failed"));
    }

    IPAddress readIPFromEEPROM() {
        byte ip[4];
        for (int n = 0; n < 4; n++) {
            ip[n] = EEPROM.read(startingAddress + n);
        }
        return IPAddress(ip[0], ip[1], ip[2], ip[3]);
    }

    void listDir(const char * dirname, uint8_t levels) {
        Serial.print("Listing SPIFFS directory: ");
        Serial.println(dirname);

        File root = SPIFFS.open(dirname);
        if (!root) {
            Serial.println("- failed to open directory");
            return;
        }
        if (!root.isDirectory()) {
            Serial.println(" - not a directory");
            return;
        }

        File file = root.openNextFile();
        while (file) {
            if (file.isDirectory()) {
                Serial.print("  DIR : ");
                Serial.println(file.name());
                if (levels) {
                    listDir(file.name(), levels - 1);
                }
            } else {
                Serial.print("  FILE: ");
                Serial.print(file.name());
                Serial.print("\tSIZE: ");
                Serial.println(file.size());
            }
            file = root.openNextFile();
        }

        Serial.println("");
    }

    void removeConfigFile(String config_filename) {
        if (SPIFFS.exists(config_filename)) {
            Serial.print("Removing: ");
            Serial.println(config_filename);
            if (!SPIFFS.remove(config_filename))
                Serial.println("Error removing preferences");
        } else {
            Serial.println("No saved preferences file to remove");
        }
    }

    void dumpFile(String config_filename, int max_char_dump) {
        if (SPIFFS.exists(config_filename)) {
            File file = SPIFFS.open(config_filename, FILE_READ);
            int countSize = 0;
            while (file.available() && countSize <= max_char_dump) {
                Serial.print(char(file.read()));
                countSize++;
            }
            Serial.println("");
            file.close();
        } else {
            Serial.print(config_filename);
            Serial.println(" not found, nothing to dump.\n");
        }
    }

    String readFile(String filename) {
        Serial.println("readFile -> Reading file: " + filename);

        File file = SPIFFS.open(filename);
        if (!file || file.isDirectory()) {
            Serial.println("readFile -> failed to open file for reading; leaving it intact");
            return "";
        }

        String fileText = "";
        while (file.available()) fileText = file.readString();

        file.close();
        return fileText;
    }

    bool writeFile(String filename, String message) {
        Serial.println("writeFile -> Writing file: " + filename);

        File file = SPIFFS.open(filename, FILE_WRITE);

        if (!file) {
            Serial.println("writeFile -> failed to open file for writing");
            return false;
        }
        if (file.print(message)) {
            Serial.println("writeFile -> file written");
            file.close();
            return true;
        } else {
            Serial.println("writeFile -> write failed");
            file.close();
            return false;
        }
    }

}
