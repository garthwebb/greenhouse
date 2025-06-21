#ifndef EXTERNALSETTINGS_H
#define EXTERNALSETTINGS_H

#include <Arduino.h>

#include "Logger.h"
#include "ArduinoJson.h"

class ExternalSettings {
    private:
	String _last_modified = "";
	JsonDocument _doc;

    public:

    ExternalSettings(String host, uint16_t port, String path);

	void monitor();

	template <typename T>
	T get(const String &key, T defaultValue) {
		if (_doc.isNull()) {
			Serial.println("Error: JSON document is not initialized.");
			return defaultValue;
		}

		if (_doc[key].isNull()) {
			Serial.printf("Warning: Key '%s' not found in JSON document, returning default value\n", key.c_str());
			return defaultValue;
		} else {
			return _doc[key].as<T>();
		}
	}
};

#endif