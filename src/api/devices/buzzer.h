#pragma once

#include <Arduino.h>
#include <vector>

namespace Esp32
{
    enum BUZZER_state
    {
        eON,
        eOFF,        
        eINTRO,
        eGOAL,
        ePERIOD_BELL
    };
    
    BUZZER_state state = BUZZER_state::eOFF;
    



    class Buzzer
    {
    public:
        Buzzer()
        {}

        ~Buzzer()
        {}

        void setup() 
        {
           

        }

        void loop() 
        {
            switch(state) 
            {
                case BUZZER_state::eINTRO:

                        playSiren();               
                        state = BUZZER_state::eOFF;
                        break;

                case BUZZER_state::eON:
                        break;
                case BUZZER_state::eOFF:
                        break;
            }

        }


        const int speakerPin = 14;


        void playSiren()
        {
            /*  // Play a crescendo siren sound
                for (int freq = 100; freq <= 1000; freq += 50) {
                    tone(speakerPin, freq, 50);  // Increase frequency gradually every 50 milliseconds
                    delay(50);  // Delay for smooth transition
                }
                for (int freq = 1000; freq >= 100; freq -= 50) {
                    tone(speakerPin, freq, 50);  // Decrease frequency gradually every 50 milliseconds
                    delay(50);  // Delay for smooth transition
                }*/

            const int quart = 250; 
            const int half = 500;
            const int quart3 = 750;
            const int note = 1000;
            const int note2 = 2000;


            tone(speakerPin, 523, half); //C
            delay(half);
            
            tone(speakerPin, 784/2, half); //G
            delay(half);
            tone(speakerPin, 440, half); //A
            delay(half);
            tone(speakerPin, 494, half); //B
            delay(half);
            tone(speakerPin, 523, half); //C
            delay(half);
            
            tone(speakerPin, 784/2, half); //G
            delay(half);
            tone(speakerPin, 440, half); //A
            delay(half);
            tone(speakerPin, 494, half); //B
            delay(half);
            tone(speakerPin, 555, half); //Db
            delay(half);
            
            tone(speakerPin, 408, half); //Ab
            delay(half);
            tone(speakerPin, 462, half); //Bb
            delay(half);
            tone(speakerPin, 523, half); //C
            delay(half);
            tone(speakerPin, 555, half); //Db
            delay(half);
            
            tone(speakerPin, 408, half); //Ab
            delay(half);
            tone(speakerPin, 462, half); //Bb
            delay(half);
            tone(speakerPin, 523, half); //C
            delay(half);
            tone(speakerPin, 587, half); //D
            delay(half);
            
            tone(speakerPin, 440, half); //A
            delay(400);
            tone(speakerPin, 494, half); //B
            delay(400);
            tone(speakerPin, 555, half); //Db
            delay(400);
            tone(speakerPin, 587, half); //D
            delay(400);
            
            tone(speakerPin, 440, half); //A
            delay(400);
            tone(speakerPin, 494, half); //B
            delay(400);
            tone(speakerPin, 555, half); //Db
            delay(400);
            tone(speakerPin, 587, note2); //D
            delay(note);
            

            tone(speakerPin, 440, 300); //A
            delay(350);
            tone(speakerPin, 587, 350); //D
            delay(350);
            tone(speakerPin, 738, quart); //F#
            delay(half);
            tone(speakerPin, 440*2, half); //A
            delay(note);
            tone(speakerPin, 738, quart); //F#
            delay(half);
            tone(speakerPin, 440*2, note); //A
            delay(1500);
            tone(speakerPin, 587, 1500); //D
            delay(half);

            /*tone(speakerPin, 462, 300); //Bb
            delay(quart);
            tone(speakerPin, 627, 350); //Eb
            delay(quart);
            tone(speakerPin, 784/2, quart); //G
            delay(half);
            tone(speakerPin, 462, half); //Bb
            delay(1500);
            tone(speakerPin, 784/2, quart); //G
            delay(half);
            tone(speakerPin, 462, note); //Bb
            delay(1500);
            tone(speakerPin, 627, 1500); //Eb
            delay(half);

            tone(speakerPin, 494, 300); //B
            delay(quart);
            tone(speakerPin, 659, 350); //E
            delay(quart);
            tone(speakerPin, 408*2, 350); //Ab
            delay(half);
            tone(speakerPin, 494, half); //B
            delay(note);
            tone(speakerPin, 408*2, quart); //Ab
            delay(half);
            tone(speakerPin, 494, note); //B
            delay(note);
            tone(speakerPin, 659, 1500); //E
            delay(half);

            tone(speakerPin, 523, 300); //C
            delay(quart);
            tone(speakerPin, 698, 350); //F
            delay(quart);
            tone(speakerPin, 440*2, 350); //A
            delay(half);
            tone(speakerPin, 523, half); //C
            delay(note);
            tone(speakerPin, 440*2, quart); //A
            delay(half);
            tone(speakerPin, 523, note); //C
            delay(note);
            tone(speakerPin, 698, 2500); //F*/


            /*
            tone(speakerPin, 440, 2000); //A
            tone(speakerPin, 494, 2000); //B
            tone(speakerPin, 523, 2000); //C
            tone(speakerPin, 587, 2000); //D
            tone(speakerPin, 659, 2000); //E
            tone(speakerPin, 698, 2000); //F
            tone(speakerPin, 784, 2000); //G
            tone(speakerPin, 880, 2000); //A
            */
            //Playing the melodies an octave higher  -  tone(speakerPin, 494*2, 2000);
            //You can use the notone() function instead of the delay()
        }

    private:
        
    

        
    };
  

}
