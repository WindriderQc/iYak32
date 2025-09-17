#pragma once

#include <EEPROM.h>
#include "SPIFFS.h"


namespace Storage {

   




    int startingAddress = 0; // New starting address for IP in EEPROM

    void IPtoEEPROM(IPAddress IPaddr) 
    {
        Serial.print("Saving IP to EEPROM : ");
        Serial.println(IPaddr);

        /*for (int n = 0; n < sizeof(ipchar); n++) // automatically adjust for number of digits        {
            EEPROM.write(n + startingAddress, ipchar[n]);
        }*/
        for (int n = 0; n < 4; n++) // adjust for number of digits
        {
            EEPROM.write(startingAddress + n , IPaddr[n]);
            Serial.println( IPaddr[n]);
        }

        EEPROM.commit() ? Serial.println(F("\nEEPROM successfully committed")) : Serial.println(F("\nERROR! EEPROM commit failed"));
    }

    IPAddress readIPFromEEPROM() {
        int startingAddress = 0; // Same starting address
        byte ip[4];
        for (int n = 0; n < 4; n++) {
            ip[n] = EEPROM.read(startingAddress + n); // Read IP starting at address 10
        }
        return IPAddress(ip[0], ip[1], ip[2], ip[3]);
    }


    //////////////////////////////////////////////
    // FileSystem features
    /////////////////////////////////////////////
    /*
        Internal filesystem (SPIFFS) used for non-volatile settings
    */

  

    void listDir(const char * dirname, uint8_t levels)
    {
        Serial.print("Listing SPIFFS directory: "); Serial.println(dirname);

        File root = SPIFFS.open(dirname);
        if(!root){
            Serial.println("- failed to open directory");
            return;
        }
        if(!root.isDirectory()){
            Serial.println(" - not a directory");
            return;
        }

        File file = root.openNextFile();
        while(file){
            if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(file.name(), levels -1);
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


    void removeConfigFile(String config_filename) 
    {
        if (SPIFFS.exists(config_filename)) 
        {
            Serial.print("Removing: "); Serial.println(config_filename);
            if (!SPIFFS.remove(config_filename))  
                Serial.println("Error removing preferences");

        } else  Serial.println("No saved preferences file to remove");  
    }


    void dumpFile(String config_filename, int max_char_dump = 1024)
    {
        if (SPIFFS.exists(config_filename)) {
            // Dump contents for debug
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


    String readFile(String filename)
    {
        Serial.println("readFile -> Reading file: " + filename);

        File file = SPIFFS.open(filename);
        if(!file || file.isDirectory()){
            Serial.println("readFile -> failed to open file for reading, maybe corrupt, removing");
            removeConfigFile(filename);
            return "";
        }

        String fileText = "";

        while(file.available())  fileText = file.readString(); 
        
        file.close();
        return fileText;
    }


    bool writeFile(String filename, String message)
    {
        Serial.println("writeFile -> Writing file: " + filename);

        File file = SPIFFS.open(filename, FILE_WRITE);
        
        if(!file)                   { Serial.println("writeFile -> failed to open file for writing");  return false; }
        if(file.print(message))     { Serial.println("writeFile -> file written");  file.close();      return true;  }
        else                        { Serial.println("writeFile -> write failed");  file.close();      return false; }   
    }

}