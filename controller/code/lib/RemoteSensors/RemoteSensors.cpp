#include "RemoteSensors.h"
#include "ArduinoJson.h"
#include "WirelessControl.h"

extern Logger *LOGGER;

RemoteSensors::RemoteSensors(const char *host, uint16_t port, const char *client_id) : _mqtt(_wifi_client), _client_id(client_id) {
	LOGGER->log("Initializing RemoteSensors");

	_mqtt.setServer(host, port);
	_mqtt.setBufferSize(MQTT_BUFFER_SIZE);
	_mqtt.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
		this->_on_message(topic, payload, length);
	});

	// Make sure we try to connect immediately
	_last_connect_attempt_ms = millis() - MQTT_RECONNECT_PERIOD_MS;
	monitor();
}

void RemoteSensors::monitor() {
	if (!WirelessControl::is_connected) {
		return;
	}

	if (_mqtt.connected()) {
		_mqtt.loop();
		return;
	}

	if (millis() - _last_connect_attempt_ms < MQTT_RECONNECT_PERIOD_MS) {
		return;
	}
	_last_connect_attempt_ms = millis();

	_connect();
}

void RemoteSensors::_connect() {
	if (!_mqtt.connect(_client_id.c_str())) {
		LOGGER->log_error("MQTT connection failed, state " + String(_mqtt.state()));
		return;
	}

	// Readings are retained, so subscribing gets us the latest from every sensor right away
	_mqtt.subscribe(REMOTE_SENSOR_TOPIC);
	LOGGER->log("Connected to MQTT broker, subscribed to " + String(REMOTE_SENSOR_TOPIC));
}

bool RemoteSensors::is_connected() {
	return _mqtt.connected();
}

void RemoteSensors::_on_message(char *topic, uint8_t *payload, unsigned int length) {
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, payload, length);
	if (error) {
		LOGGER->log_error("Bad remote sensor message on " + String(topic) + ": " + String(error.f_str()));
		return;
	}

	const char *id = doc["id"];
	if (id == nullptr) {
		LOGGER->log_error("Remote sensor message on " + String(topic) + " has no id");
		return;
	}

	if (_readings.find(id) == _readings.end()) {
		LOGGER->log("Remote sensor first seen: " + String(id));
	}

	RemoteReading &reading = _readings[id];
	reading.temperature = doc["temperature"] | NAN;
	reading.humidity = doc["humidity"] | NAN;
	reading.battery_v = doc["battery_v"] | NAN;
	reading.battery_pct = doc["battery_pct"] | NAN;
	reading.rssi = doc["rssi"] | 0;
	reading.missed = doc["missed"] | 0;
	reading.ts = doc["ts"] | 0;

	// Only log when a sensor crosses into or out of low battery, not on every reading
	bool low_battery = reading.battery_pct < REMOTE_LOW_BATTERY_PCT || reading.battery_v < REMOTE_LOW_BATTERY_V;
	if (low_battery && !reading.low_battery) {
		LOGGER->log_warn("Remote sensor " + String(id) + " battery low: " + String(reading.battery_v, 3) + "V, " +
		                 String(reading.battery_pct, 1) + "% remaining");
	} else if (!low_battery && reading.low_battery) {
		LOGGER->log("Remote sensor " + String(id) + " battery no longer low; replaced?");
	}
	reading.low_battery = low_battery;
}

const std::map<std::string, RemoteReading>& RemoteSensors::readings() const {
	return _readings;
}

long RemoteSensors::age_s(const RemoteReading &reading) const {
	return (long)(time(nullptr) - reading.ts);
}

bool RemoteSensors::is_fresh(const RemoteReading &reading) const {
	return reading.ts > 0 && age_s(reading) <= REMOTE_SENSOR_STALE_S;
}
