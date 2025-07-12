#ifndef TEMPSENSOR_H
#define TEMPSENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <vector>

#include "Logger.h"

#define ONE_WIRE_SETTLE_MILLIS 500

class Sensor {
    private:

    public:
	float temp;
	byte address[8];

    Sensor(byte addr[8]);
	String get_address_string() const;

	static String byteArrayToString(const byte address[8]);
};

class SensorHandler {
    private:
	OneWire _one_wire;
	DallasTemperature _sensor_interface;

    public:
	std::vector<Sensor> sensors;

    SensorHandler();
	void scan();
	void load_readings();
};

#endif