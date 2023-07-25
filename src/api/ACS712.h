#ifndef ACS712_h
#define ACS712_h

#include <Arduino.h>

#define ADC_SCALE 1023.0
#define VREF 5.0
#define DEFAULT_FREQUENCY 50

enum ACS712_type {ACS712_05B, ACS712_20A, ACS712_30A};


/* 
Current Sensor
    // calibrate() method calibrates zero point of sensor,
    // It is not necessary, but may positively affect the accuracy
    // Ensure that no current flows through the sensor at this moment
    // If you are not sure that the current through the sensor will not leak during calibration - comment out this method
    Serial.println("Calibrating... Ensure that no current flows through the sensor at this moment");
    int zero = sensor.calibrate();
    Serial.println("Done!");
    Serial.println("Zero point for this sensor = " + zero);

    // Read current from sensor
    float I = sensor.getCurrentDC();  Serial.printf("Current(I) = %f A\n", I);

*/
class ACS712 {
public:
	ACS712(ACS712_type type, uint8_t _pin);  //  type selection will adjust sensor sensitivity
	int calibrate();
	void setZeroPoint(int _zero);
	void setSensitivity(float sens);
	float getCurrentDC();
	float getCurrentAC(uint16_t frequency = 60);  //  default to 60hz in canada/usa

private:
	int zero = 512;
	float sensitivity;
	uint8_t pin;
};

#endif
