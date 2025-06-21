#include "TempSensor.h"

extern Logger *LOGGER;

Sensor::Sensor(byte addr[8]) {
	for (int i = 0; i < 8; i++) {
		address[i] = addr[i];
	}
}

String Sensor::get_address_string() const {
	return Sensor::byteArrayToString(address);
}

String Sensor::byteArrayToString(const byte address[8]) {
    String address_string = "";
    for (int i = 0; i < 8; i++) {
        if (address[i] < 16) address_string += "0";
        address_string += String(address[i], HEX);
    }
    return address_string;
}

SensorHandler::SensorHandler(): _one_wire(ONE_WIRE_BUS_PIN), _sensor_interface(&_one_wire) {
	// Start up the library
	_sensor_interface.begin();

	scan();
}

void SensorHandler::scan() {
	// Clear the sensors vector
	sensors.clear();

	byte address[8];
	while (_one_wire.search(address)) {
		Serial.printf("Found device with address: %s\n",
			Sensor::byteArrayToString(address).c_str());
		LOGGER->log("Found temperature device with address: " + Sensor::byteArrayToString(address));

		sensors.push_back(Sensor(address));
	}

	// Reset search for next loop
	_one_wire.reset_search();

	Serial.printf("Found %d devices.\n", sensors.size());
}

void SensorHandler::load_readings() {
	_sensor_interface.requestTemperatures();
	delay(ONE_WIRE_SETTLE_MILLIS);

	for (auto &sensor : sensors) {
		sensor.temp = _sensor_interface.getTempC(sensor.address);
		Serial.printf("Sensor %s: %f\n", sensor.get_address_string().c_str(), sensor.temp);
	}
}
