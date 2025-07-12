#ifndef EXTERNALSETTINGS_H
#define EXTERNALSETTINGS_H

#include <Arduino.h>

#include "Logger.h"
#include "ArduinoJson.h"

// Temp we want the greenhouse
#define DEFAULT_TARGET_TEMP_F 70.0

// Max and min define when windows & fan should always be open/on or always be closed/off
#define DEFAULT_MAX_TEMP_F 80.0
#define DEFAULT_MIN_TEMP_F 60.0

#define DEFAULT_TEMP_SHORT_DETLA_S 5*60
#define DEFAULT_TEMP_LONG_DETLA_S 60*60

// Humidity we want the greenhouse
#define DEFAULT_TARGET_HUMIDITY 60.0
#define DEFAULT_MIST_ON_S 30
#define DEFAULT_MIST_OFF_S 2*60

class ExternalSettings {
    private:
	String _last_modified = "";
	JsonDocument _doc;
	String _host;
	uint16_t _port;
	String _path;

	void _reset_connection();

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

	// Explicitly provide methods for all expected values
	float get_target_temp_f() {
		return get<float>("target_temp_f", DEFAULT_TARGET_TEMP_F);
	}

	float get_max_temp_f() {
		return get<float>("max_temp_f", DEFAULT_MAX_TEMP_F);
	}

	float get_min_temp_f() {
		return get<float>("min_temp_f", DEFAULT_MIN_TEMP_F);
	}

	int get_temp_long_delta_s() {
		return get<int>("temp_long_detla_s", DEFAULT_TEMP_LONG_DETLA_S);
	}

	int get_temp_short_delta_s() {
		return get<int>("temp_short_detla_s", DEFAULT_TEMP_SHORT_DETLA_S);
	}

	float get_target_humidity() {
		return get<float>("target_humidity", DEFAULT_TARGET_HUMIDITY);
	}

	int get_mist_on_s() {
		return get<int>("mist_on_s", DEFAULT_MIST_ON_S);
	}

	int get_mist_on_ms() {
		return get_mist_on_s() * 1000;
	}

	int get_mist_off_s() {
		return get<int>("mist_off_s", DEFAULT_MIST_OFF_S);
	}

	int get_mist_off_ms() {
		return get_mist_off_s() * 1000;
	}
};

#endif