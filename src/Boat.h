#pragma once

#include <Arduino.h>
#include <vector>
#include <ESP32_Servo.h> 


namespace Boat  
{
    class Boat
    {
    public:
        Boat()
        {}

        ~Boat()
        {}

        void setup(int servoPin, int RPWM, int LPWM, int ADC_Max) 
        {
            Esp32::configPin(DIRECTION, "IN", "DIRECTION", true);
            Esp32::configPin(SPEED, "IN", "SPEED", true);
            Esp32::configPin(FWD, "INPULL", "FWD");
            Esp32::configPin(BCK, "INPULL", "BCK");


             //myservo.attach(SERV); 
            myservo.attach(servoPin, SERVO_MIN, SERVO_MAX);   // attaches the servo on pin XX to the servo object
                                         // using SG90 servo min/max of 500us and 2400us
                                         // for MG995 large servo, use 1000us and 2000us,
                                         // which are the defaults, so this line could be
                                         // "myservo.attach(servoPin);"

            // configure MOTOR PWM functionalitites
            ledcSetup(ledChannelR, freq, resolution);
            ledcSetup(ledChannelL, freq, resolution);
        
            // attach the channel to the GPIO to be controlled
            ledcAttachPin(RPWM, ledChannelR);
            ledcAttachPin(LPWM, ledChannelL);

            ADC_max_ = ADC_Max;
        }


        void loop() 
        {
          
            int fwd = digitalRead(FWD);
            int bck = digitalRead(BCK);
            int s =  analogRead(SPEED);  //  0 - 4095 
            int d =  analogRead(DIRECTION);  //  0 - 4095 
                   
                    
            if(millis()-boat_timestamp > SERVO_INTERVAL) {
                boat_timestamp += SERVO_INTERVAL;                 

                if( abs(prevDirPin_ - d) > DIR_POT_BUFFER ) {  //  pot value tends to fluctuate a little.  Create a buffer to change servo only on real dir input
                
                    prevDirPin_ = d;
                    int dir = map(d, 0, Esp32::ADC_Max, 180, 0);   //  OUPS ...  hardware pot is inverted...   //   Servo.write is limited to 180 so 180 = 270 for capable servo
                    setDir(dir);
                }
                  
                setSpeed(fwd, bck, s);  
            }

        }

        int getSpeed() { return speed_; }
        int getDir() { return direction_; }

        void setSpeed(int fwd, int bck, int speed ) {
            speed_ = speed; 
            
            if(fwd && bck) {//Active low input so if both inactive
                    // STOP
                    ledcWrite(ledChannelR, 0);
                    ledcWrite(ledChannelL, 0); 
            }
            else  {  
                if(fwd == LOW) {
                    // FORWARD
                    ledcWrite(ledChannelR, speed_);
                    ledcWrite(ledChannelL, 0); 
                }
                else if(bck == LOW) {
                    // BACKWARD
                    ledcWrite(ledChannelL,  speed_);
                    ledcWrite(ledChannelR, 0);   
                }
            }

        }
        
        void setDir(int dir) { 
            direction_ = dir;
        
            // Increment or decrement the current angle according to the set_angle
            // and don't change it, when we already are at the set_angle
            if(direction_ > current_angle) {
                current_angle = current_angle + SERVO_SPEED_FACTOR;
            } else if(direction_ < current_angle) {
                current_angle = current_angle - SERVO_SPEED_FACTOR;
            }
            // Write the new angle to the servo
            //  myservo.writeMicroseconds(map(current_angle, 0, 270, SERVO_MIN, SERVO_MAX));  
            myservo.write(current_angle);
        }


        int pressure = 0;
        unsigned long boat_timestamp = 0;

        const int SERVO_INTERVAL  =  10;   // changing the servo position every 10ms, defines the speed of the servo
        const int DIR_POT_BUFFER = 10;

            //  Pins definition
        const int DIRECTION =34;  // A2
        const int SPEED =39;   // A3

        const int RPWM =15;  // IO15   
        const int LPWM =14;  //IO14   
        const int FWD =18; //  mosi
        const int BCK =19; // miso 

        const int SERVO_PIN =27; // IO27


    private:
        const int SERVO_SPEED_FACTOR  =  2;   // changing the servo position of FACTOR degrees every INTERVAL
        const int SERVO_MIN = 500;     
        const int SERVO_MAX = 2500;

        /******************************************
        Control Specification  DS 5160 SSG 
        Control System: PWM(Pulse width modification)
        Pulse width range: 500～2500µsec
        Neutral position: 1500µsec
        Running degree: 270° (when 500～2500 μ sec)
        Dead band width: 3 µsec
        Operating frequency: 50-330Hz
        Rotating direction: Counterclockwise (when 500～2500 µsec)
        ********************************************/
        //Frequency  Bit depth     Available steps for transitions
        //19531Hz       12              4096

        // setting PWM properties
        const int freq = 20000; //  25Khz max pour BTS7960    40Mhz max for esp32
        const int ledChannelR = 6;
        const int ledChannelL = 2;
        const int resolution = 8;  
       

        int ADC_max_ = 4095;
        int speed_ = 0;
        int direction_ = 0;
        int prevSpeedPin_;
        int  prevDirPin_;

        Servo myservo; 

        int set_angle = 0;
        int current_angle = 90; // setting starting angle to 90 degrees
    };

    
}

Boat::Boat boat = Boat::Boat();