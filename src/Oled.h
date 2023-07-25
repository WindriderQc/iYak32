#pragma once
/* 
Add to platformio.ini:
    lib_deps =
        Adafruit GFX Library  ; required for Oled display
        Adafruit SSD1306      ; required for Oled display
*/
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

namespace Oled {

  #define OLED_WIDTH 128 // OLED display width, in pixels
  #define OLED_HEIGHT 64 // OLED display height, in pixels
  #define OLED_LINES   8
  #define OLED_I2C_ADDRESS 0x3C

  // Declaration for SSD1306 display connected using I2C:
  Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1, 800000U);  // 800000U => ESP32 can use I2C up to 800KHz when Oled device supports it

  // Declaration for SSD1306 display connected using software SPI:
  /*
  #define OLED_MOSI  33
  #define OLED_CLK   15
  #define OLED_DC    12
  #define OLED_CS    32
  #define OLED_RESET 27
 
  Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
  */

    bool isConnected()
    {
        Wire.beginTransmission((uint8_t)OLED_I2C_ADDRESS);
        
        if (Wire.endTransmission() == 0) //See if something ack's at this address
        {
            return true;
            /*if (getID() == LPS25HB_DEVID) //Make sure ID is what we expect
            {
                return true;
            }
            return false;*/
        }
        return false;
    }

    bool setupOled() 
    {

        if(!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // I2C Address = 0x3C   //  don't put the address to initialize with SPI
            Serial.println(F("SSD1306 allocation failed"));
            return(false);
        }
        else {
            if(isConnected())
            {
                Serial.println(F("Oled Display initialized."));
                // Show initial display buffer contents on the screen -- the library initializes this with Adafruit splash screen.
                oled.display();
                return(true);
            }
            else {
                Serial.println(F("Oled Display not connected."));
                return(false);
            }

        
            }
    
    }




  void testOledDrawStyles(void) 
  {
    oled.clearDisplay();

    oled.setTextSize(1);             // Normal 1:1 pixel scale
    oled.setTextColor(WHITE);        // Draw white text
    oled.setCursor(0,0);             // Start at top-left corner
    oled.println(F("Hello, world!"));

    oled.setTextColor(BLACK, WHITE); // Draw 'inverse' text
    oled.println(3.141592);

    oled.setTextSize(2);             // Draw 2X-scale text
    oled.setTextColor(WHITE);
    oled.print(F("0x")); oled.println(0xDEADBEEF, HEX);

    oled.display();
  }
 

}
/*  
oled.drawPixel(10, 10, WHITE);// Draw a single pixel in white
display.drawLine(x1, y1, x2, y2, WHITE)    //  display.height()-1, WHITE);
display.drawRect(i, i, display.width()-2*i, display.height()-2*i, WHITE);
display.display(); // Update screen with each newly-drawn rectangle
display.fillRect(i, i, display.width()-i*2, display.height()-i*2, INVERSE);
display.clearDisplay();
display.drawCircle(display.width()/2, display.height()/2, i, WHITE);
display.fillCircle(display.width() / 2, display.height() / 2, i, INVERSE);
display.drawRoundRect(i, i, display.width()-2*i, display.height()-2*i,  display.height()/4, WHITE);
display.fillRoundRect(i, i, display.width()-2*i, display.height()-2*i,        display.height()/4, INVERSE);
display.drawTriangle(
display.width()/2  , display.height()/2-i,
display.width()/2-i, display.height()/2+i,
display.width()/2+i, display.height()/2+i, WHITE);
*/