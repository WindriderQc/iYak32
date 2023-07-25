#include <Arduino.h>

//#include <TM1637Display.h>
#include "api/SevenSegmentAscii.h"

//#include "driver/ledc.h"
#include "api/Esp32.h"
#include "api/WifiManager.h"
#include "Oled.h"
#include "BMX280.h"
#include "Boat.h"
#include "www.h"


//  Pins definition
#define DIRECTION 34  // A2
#define SPEED 39   // A3

#define RPWM 15  // IO15   
#define LPWM 14  //IO14   
#define FWD 18 //  mosi
#define BCK 19  // miso 

#define SERVO_PIN 27 // IO27

// TM1637 Module connection pins (Digital Pins)
#define CLK 32
#define DIO 33

//  22 - 23  //  I2C comm  SDA/SCL





const int ADC_Max = 4095;    
const int POT_BUFFER = 10;

unsigned long boat_timestamp = 0;



TM1637Display display(CLK, DIO);
// Create an instance of the SevenSegmentAscii library
SevenSegmentAscii asciiDisplay(display, 5);  // Set the brightness level (0-7)



WifiManager wifiManager;



int speed, prevDirPin;

bool oledConnected = false;
bool bmxConnected = false;

//  STATE Machine - System 
enum class SYS_state {
    BOOT,
    DEVICES_CONFIG,
    HEATUP,
    FIRSTLOOP,
    LOOP
};
SYS_state state = SYS_state::BOOT;


unsigned long timeSinceBoot = 0;

void setup() {
 
    Serial.begin(115200);

    Esp32::configPin(LED_BUILTIN, "OUT", "Builtin LED");
    Esp32::ioBlink(LED_BUILTIN,500, 750, 4);
  
    Esp32::configPin(DIRECTION, "IN", "DIRECTION", true);
    Esp32::configPin(SPEED, "IN", "SPEED", true);
    Esp32::configPin(FWD, "INPULL", "FWD");
    Esp32::configPin(BCK, "INPULL", "BCK");
    
    Esp32::i2cScanner();

    oledConnected = Oled::setupOled();  
    bmxConnected  = BMX280::init();
            

    boat.setup(SERVO_PIN, RPWM, LPWM);
    boat.setDir(135);
    boat.setSpeed(1,1,0);    
    
    wifiManager.begin();
    www::setup();

   
    asciiDisplay.displayString("u1:0 ");

    Serial.println(F("Setup completed.    -     Launching loop and State Machine..."));
}




void loop() 
{
 
    timeSinceBoot += millis();
    Esp32::loop();
   
    switch(state) 
    {
    case SYS_state::BOOT:

            Serial.println("\nBOOT done\n");
            state = SYS_state::DEVICES_CONFIG;
            break;

    case SYS_state::DEVICES_CONFIG:
           
            //setupTimerz();
           // setupSensors();
            //setupAlarmsClock();

            Serial.println("\nSENSORS & ALARMLib CONFIG done\n");
            state = SYS_state::HEATUP;
            break;
    
    case SYS_state::HEATUP:
            Serial.print("Heating up...");
           // insure all sensor at heated up to send accurate data at start
            if(timeSinceBoot <= 3000) {
                Serial.print("."); Serial.print(timeSinceBoot); Serial.print("."); 
            }  
            else {
                state = SYS_state::FIRSTLOOP; Serial.println("Done");
                }
            break;

    case SYS_state::FIRSTLOOP:

            Serial.println();
           // Esp32::getBattRemaining(true);
            if(bmxConnected) BMX280::actualizeWeather(true);
         
            //Lux::loop(); 
            
            
            Serial.println("FIRST LOOP done");
            Serial.println("Let's roll!\n");
            state = SYS_state::LOOP;
            break;

    case SYS_state::LOOP:       

            ////////////  YakLoop  //////////////////////////
          
            int fwd = digitalRead(FWD);
            int bck = digitalRead(BCK);
            int s =  analogRead(SPEED);  //  0 - 4095 


            int d =  analogRead(DIRECTION);  //  0 - 4095 
                   
                    
            if(millis()-boat_timestamp > boat.SERVO_INTERVAL) {
                boat_timestamp += boat.SERVO_INTERVAL;                 

                if( abs(prevDirPin - d) >= POT_BUFFER ) {  //  pot value tends to fluctuate a little.  Create a buffer to change servo only on real dir input
                
                    prevDirPin = d;
                    int dir = map(d, 0, ADC_Max, 180, 0);   //  OUPS ...  hardware pot is inverted...   //   Servo.write is limited to 180 so 180 = 270 for capable servo
                     boat.setDir(dir); 
                   // Serial.printf("dir: %d", dir);  
                
                }
                  
                boat.setSpeed(fwd, bck, s);  
            }
            ////////////////////////////////////////////////



            static unsigned long startTime1 = 0;
            static unsigned long startTime5 = 0;

            if (millis() - startTime1 >= 1000)
            {
    
                if(oledConnected) {
                    Oled::oled.clearDisplay();
              
                    Oled::oled.setTextSize(1);             
                    Oled::oled.setCursor(0,0);             // Start at top-left corner
                    Oled::oled.setTextColor(WHITE);  
                    Oled::oled.printf("Speed:%d\n",boat.getSpeed());            
                    Oled::oled.printf("Dir: %d\n", boat.getDir()); 
                    Oled::oled.setTextSize(1);  
                    Oled::oled.println(wifiManager.getIP());
                    Oled::oled.printf("\nKPa: %.2f", BMX280::pressure); 
                    Oled::oled.printf("\ntC: %.1f", BMX280::temp);
                    Oled::oled.printf("\nAlt: %.1f", BMX280::altitude);

                    Oled::oled.display(); 
                }
   
                  
                startTime1 = millis();  
            }

            if (millis() - startTime5 >= 5000)
            {
                if(bmxConnected) BMX280::actualizeWeather();  
                //Serial.println(BMX280::pressure);
               // Lux::loop(); 
                startTime5 = millis(); 
            }


            wifiManager.loop(); 
            break;
    }

}