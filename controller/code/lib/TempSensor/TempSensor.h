#ifndef TEMPSENSOR_H
#define TEMPSENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <vector>

#include "monitor.h"
#include "Logger.h"

class Sensor {
    public:
	float temp;
	byte address[8];

    Sensor(byte addr[8]);
	String get_address_string() const;

	static String byteArrayToString(const byte address[8]);

    private:

};

class SensorHandler {
    public:
	std::vector<Sensor> sensors;

    SensorHandler();
	void scan();
	void load_readings();

    private:
	OneWire _one_wire;
	DallasTemperature _sensor_interface;
};

#endif