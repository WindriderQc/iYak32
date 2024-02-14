#pragma once

#include "Arduino.h"

// TODO:  Use delay().  will affct timeAlarm class

/* Read the values from the sensor, it returns an array which contains 3 values.
	* 1 = LPG in ppm
	* 2 = CO in ppm
	* 3 = SMOKE in ppm
*/
/*
        //float* values= mq2.read(false); //set it false if you don't want to print the values in the Serial
        //mq2.read(false);

        //lpg = values[0];
        lpg = mq2.readLPG();
        //co2 = values[1];
        co2 = mq2.readCO();
        //smoke = values[2];
        smoke = mq2.readSmoke();   

*/

	class MQ2 
	{
		public: 
			MQ2();
			float* read(bool print);
			float readLPG();
			float readCO();
			float readSmoke();
			float begin(int pin);
		private:
			int _pin;
			int RL_VALUE = 5;     //define the load resistance on the board, in kilo ohms
			int RO_CLEAN_AIR_FACTOR = 9.83;  
			int CALIBARAION_SAMPLE_TIMES = 5; 
			int CALIBRATION_SAMPLE_INTERVAL = 50;
			int READ_SAMPLE_INTERVAL = 50;
			int READ_SAMPLE_TIMES = 5;

			float LPGCurve[3]  =  {2.3,0.21,-0.47}; 
			float COCurve[3]  =  {2.3,0.72,-0.34};   
			float SmokeCurve[3] = {2.3,0.53,-0.44};                                                       
			float Ro = 10;             
		
			int GAS_LPG = 0;
			int GAS_CO = 1;
			int GAS_SMOKE = 2;

			float lpg = 0;
			float co = 0;
			float smoke = 0;
			
			float MQRead();
			float MQGetGasPercentage(float rs_ro_ratio, int gas_id);
			int MQGetPercentage(float rs_ro_ratio, float *pcurve);
			float MQCalibration();
			float MQResistanceCalculation(int raw_adc);

			int lastReadTime = 0;
	};


