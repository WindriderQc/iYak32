#pragma once

#include <Arduino.h>


 namespace Esp32 {

    class Hourglass 
    {
        public:
            Hourglass() {}

            ~Hourglass() {}

            bool setupTimeSync() {
                  
                configTime(gmtOffset_sec, daylightOffset_sec, ntp_primary, ntp_secondary);
                
                Serial.print(F("Waiting on time sync..."));
                int j = 0;

                while (time(nullptr) < 1510644967) 
                {
                    delay(100);   //Alarm.delay(100);  
                    Serial.print(".");
                    j++;

                    if(j >=80) { 
                        Serial.println(F("timesync is down."));  
                        return false;   
                    }
                } 
                
                Serial.println(F("completed."));  

                return true;
            }


            String getDateTimeString(bool print = false, bool withMillis = false)
            {
                timeval now;
                gettimeofday(&now, nullptr);

                char utc[80];
                char utc_with_millis[84];
                strftime(utc, sizeof(utc), "%Y-%m-%d %H:%M:%S%f", localtime(&now.tv_sec));
                sprintf(utc_with_millis, "%s.%03d", utc, (int)(now.tv_usec / 1000));

                String datetime;

                if(withMillis) datetime = utc_with_millis;
                else           datetime = utc;

                if(print) Serial.println("Date : " + datetime);

                return datetime;
            }


            tm* getDateTime()
            {
                timeval n;
                gettimeofday(&n, nullptr);
                return localtime(&n.tv_sec); 
            }

            void printDigits(int digits)
            {
                Serial.print(":");
                if(digits < 10) Serial.print('0');
                Serial.print(digits);
            }

            // digital clock display of the time
            /*void digitalClockDisplay(time_t t)
            {
                Serial.print(hour(t));  printDigits(minute(t));  printDigits(second(t));  Serial.print(" ");
                Serial.print(year(t));  Serial.print("-");  Serial.print(month(t));  Serial.print("-");  Serial.print(day(t)); Serial.println();
            }*/
            void digitalClockDisplay(tm t)
            {
                Serial.print(t.tm_hour); printDigits(t.tm_min); printDigits(t.tm_sec); Serial.print(" "); 
                Serial.print(t.tm_year + 1900); Serial.print("-"); Serial.print(t.tm_mon + 1); Serial.print("-"); Serial.print(t.tm_mday); Serial.println();  // tm =  month start at 0, and minus 1900 on year
            }
        /*void digitalClockDisplay(tmElements_t t)
            {
                Serial.print(t.Hour); printDigits(t.Minute); printDigits(t.Second); Serial.print(" "); 
                Serial.print(tmYearToCalendar(t.Year)); Serial.print("-"); Serial.print(t.Month); Serial.print("-"); Serial.print(t.Day); Serial.println();  
            }*/
            void digitalClockDisplay(int h, int m, int s, int yy, int mm, int dd)
            {
                Serial.print(h); printDigits(m); printDigits(s); Serial.print(" "); 
                Serial.print(yy); Serial.print("-"); Serial.print(mm); Serial.print("-"); Serial.print(dd); Serial.println();
            }
            /*void digitalClockDisplay()  //  displaying time sync'ed in timelib
            {
                Serial.print(hour()); printDigits(minute()); printDigits(second()); Serial.print(" ");
                Serial.print(year()); Serial.print("-"); Serial.print(month()); Serial.print("-"); Serial.print(day()); Serial.println();
            }*/





            private:
                // Configuration for NTP
                const char* ntp_primary = "time.google.com";
                const char* ntp_secondary = "time.nist.gov";
                const long  gmtOffset_sec = -18000;   //  -5 * 60 * 60  Eastern time
                const int   daylightOffset_sec = 3600;
    };

 }


   
//  because of pragma_once, this will only be delared once if include in multiple h files 
//Esp32::Hourglass hourglass = Esp32::Hourglass();  //  single global instance