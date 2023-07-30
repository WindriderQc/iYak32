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

        void setup(int servoPin, int RPWM, int LPWM) 
        {
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
        }

        void loop() 
        {
           

        }

        int getSpeed() { return speed; }
        int getDir() { return direction; }

        void setSpeed(int fwd, int bck, int spd ) {
            speed = spd; 
            
            if(fwd && bck) {//Active low input so if both inactive
                    // STOP
                    ledcWrite(ledChannelR, 0);
                    ledcWrite(ledChannelL, 0); 
            }
            else  {  
                if(fwd == LOW) {
                    // FORWARD
                    ledcWrite(ledChannelR, speed);
                    ledcWrite(ledChannelL, 0); 
                }
                else if(bck == LOW) {
                    // BACKWARD
                    ledcWrite(ledChannelL,  speed);
                    ledcWrite(ledChannelR, 0);   
                }
            }

        }
        
        void setDir(int dir) { 
            direction = dir;
        
            // Increment or decrement the current angle according to the set_angle
            // and don't change it, when we already are at the set_angle
            if(dir > current_angle) {
                current_angle = current_angle + SERVO_SPEED_FACTOR;
            } else if(dir < current_angle) {
                current_angle = current_angle - SERVO_SPEED_FACTOR;
            }
            // Write the new angle to the servo
            //  myservo.writeMicroseconds(map(current_angle, 0, 270, SERVO_MIN, SERVO_MAX));  
            myservo.write(current_angle);
        }


        int pressure;

        const int SERVO_INTERVAL  =  10;   // changing the servo position every 10ms, defines the speed of the servo
        const int DIR_POT_BUFFER = 10;

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
        // setting PWM properties
        const int freq = 20000; //  25Khz max pour BTS7960    40Mhz max for esp32
        const int ledChannelR = 6;
        const int ledChannelL = 2;
        const int resolution = 8;  
       

        //Frequency  Bit depth     Available steps for transitions
        //19531Hz       12              4096

    




   
        int speed = 0;
        int direction = 0;
        int prevSpeedPin;

        Servo myservo; 

       
        int set_angle = 0;
        int current_angle = 90; // setting starting angle to 90 degrees

    };

    

}

Boat::Boat boat = Boat::Boat();