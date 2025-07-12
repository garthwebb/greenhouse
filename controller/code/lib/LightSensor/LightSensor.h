#ifndef LIGHTSENSOR_H
#define LIGHTSENSOR_H

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"

// This seems arbitrary, but keep what was used in the example
#define SENSOR_ID 2591

class LightSensor {
    private:
	Adafruit_TSL2591 _tsl;
	uint32_t _lum;
	bool _initialized = false;

    public:

    LightSensor();
	void displayInfo();
	void configure();

	void read();
	uint32_t getFullLuminosity();
	uint16_t getIR();
	uint32_t getVisible();
	float getLux();
};

#endif