#include "LightSensor.h"

LightSensor::LightSensor(): _tsl(SENSOR_ID) {
	if (_tsl.begin()) {
		Serial.println(F("Found a TSL2591 sensor"));
	  } else {
		Serial.println(F("No sensor found ... check your wiring?"));
		return;
	  }
		
	  // Display some basic information on this sensor
	  displayInfo();
	  
	  // Configure the sensor
	  configure();
}

void LightSensor::displayInfo(void) {
	sensor_t sensor;
	_tsl.getSensor(&sensor);
	Serial.println(F("------------------------------------"));
	Serial.print  (F("Sensor:       ")); Serial.println(sensor.name);
	Serial.print  (F("Driver Ver:   ")); Serial.println(sensor.version);
	Serial.print  (F("Unique ID:    ")); Serial.println(sensor.sensor_id);
	Serial.print  (F("Max Value:    ")); Serial.print(sensor.max_value); Serial.println(F(" lux"));
	Serial.print  (F("Min Value:    ")); Serial.print(sensor.min_value); Serial.println(F(" lux"));
	Serial.print  (F("Resolution:   ")); Serial.print(sensor.resolution, 4); Serial.println(F(" lux"));  
	Serial.println(F("------------------------------------"));
	Serial.println(F(""));
}

void LightSensor::configure() {
	// You can change the gain on the fly, to adapt to brighter/dimmer light situations
	//tsl.setGain(TSL2591_GAIN_LOW);   // 1x gain (bright light)
	_tsl.setGain(TSL2591_GAIN_MED);    // 25x gain
	//tsl.setGain(TSL2591_GAIN_HIGH);  // 428x gain
	
	// Changing the integration time gives you a longer time over which to sense light
	// longer timelines are slower, but are good in very low light situtations!
	//tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
	// tsl.setTiming(TSL2591_INTEGRATIONTIME_200MS);
	_tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
	// tsl.setTiming(TSL2591_INTEGRATIONTIME_400MS);
	// tsl.setTiming(TSL2591_INTEGRATIONTIME_500MS);
	// tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time (dim light)
}

void LightSensor::read() {
	_lum =  _tsl.getFullLuminosity();
}

uint32_t LightSensor::getFullLuminosity() {
	return _lum & 0xFFFF;
}

uint16_t LightSensor::getIR() {
	uint16_t ir = _lum >> 16;
	return ir;
}

uint32_t LightSensor::getVisible() {
	return getFullLuminosity() - getIR();
}

float LightSensor::getLux() {
	return _tsl.calculateLux(getFullLuminosity(), getIR());
}
