# iYak32

Kayak ESP32 controller with 270 deg servo, BTS7960 PWM to control electric motor direction and speed, controls available on physical controller via 2 potentiometers and BCK/FWD switch.

This project is to control a Minn Kota 30 trolling motor at the back of an OldTown Predator 13 fishing kayak.



-------------------------------------------------------------------------------



Available features:
- BMP280/BME280 supported on I2C channel
- OLED display supported on I2C channel to display wifi IP address, speed, direction and atmospherical pressure
- Async web server to provide interface with atmospheric data and controls in addition to physical controller
- WifiManager trying to connect to a list of preferred SSID, launching access Point server if no wifi connections are available.  
    a) config.txt file must be added to /data folder to list your preferred SSID:password 
    b) numPreferredNetworks in WifiManager.h must be adjusted accordingly to number of lines/SSID in config.txt
- ACS712 Current sensor library implemented
- 4 digits 7-segments Display library based on TM1637Display to show String, Char and Int implemented  
- Over The Air programming capabilities with platform.io implemented
    a) ESP IP address must be configured in platformio.ini and ESP must be connected a preferred wifi to be on the same network as the intended programming computer.  A first USB connection is obviously required  



 // Set the SSID (name) and password of the Access Point
    const char* apSsid = "iYak32";
    const char* apPassword = "12345678";
-------------------------------------------------------------------------------

- MQTT
    mqtt.txt  file must be in the data folder with a simple line:
user:password



Upcoming intentions:
- Add directions and speed control from webInterface
- Autolock button on webInterface to Use cellphone GPS via the Javascript within the webServer to send auto command to motor to stay in position 
- Add SEALEVELPRESSURE_HPA variable on webInterface to allow for proper altitude detection.  Possibly get it from meteorological API if internet access is available otherwise fall back on manual entry
