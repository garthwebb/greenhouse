#include <InfluxDBHandler.h>
#include "WirelessControl.h"

extern Logger *LOGGER;

InfluxDBHandler::InfluxDBHandler(const String &url, const String &db, const char *device) : _device(device) {
    LOGGER->log("Initializing InfluxDBHandler");
    Serial.println("Initialziing InfluxDB: ");

    _client = new InfluxDBClient(url, db);

    if (_client->validateConnection()) {
        Serial.println("\tConnected to InfluxDB: " + _client->getServerUrl());
        LOGGER->log("Connected to InfluxDB at " + _client->getServerUrl());
    } else {
        Serial.println("\tInfluxDB connection failed: " + _client->getLastErrorMessage());
        LOGGER->log_error("InfluxDB connection failed: " + _client->getLastErrorMessage());
    }

    if (WirelessControl::is_connected) {
        LOGGER->log("Logging events to InfluxDB is enabled");
    }
}

bool InfluxDBHandler::write_sensor_metric(const char *sensor_id, const String &measurement, float value) {
    Point sensor("weather");
    sensor.addTag("device", _device);
    sensor.addTag("sensor_id", sensor_id);
    sensor.addField(measurement, value);

    return _write_metric(&sensor);
}

bool InfluxDBHandler::write_event_metric(const String &event_type, bool state, const char *reason) {
    Point event("events");
    event.addTag("device", _device);
    event.addTag("reason", reason);
    event.addField(event_type, state);

    return _write_metric(&event);
}

bool InfluxDBHandler::_write_metric(Point *point) {
    if (!WirelessControl::is_connected) {
        return true;
    }

    if (!_client->writePoint(*point)) {
        String error_msg = last_error();
        if (error_msg.length() > 0) {
            LOGGER->log_error("Failed to write metric: " + error_msg);
            Serial.println("Failed to write point " + point->toLineProtocol() + ": " + error_msg);
        } else {
            LOGGER->log_error("Failed to write metric: Unknown InfluxDB error");
            Serial.println("Failed to write point " + point->toLineProtocol() + ": Unknown InfluxDB error");
        }
        return false;
    }

    return true;
}

bool InfluxDBHandler::event_fan_on(const char *reason) {
    return write_event_metric("fan_on", true, reason);
}

bool InfluxDBHandler::event_fan_off(const char *reason) {
    return write_event_metric("fan_on", false, reason);
}

bool InfluxDBHandler::event_window_open(const char *reason) {
    return write_event_metric("window_open", true, reason);
}

bool InfluxDBHandler::event_window_closed(const char *reason) {
    return write_event_metric("window_open", false, reason);
}

bool InfluxDBHandler::event_mist_on(const char *reason) {
    return write_event_metric("mist_on", true, reason);
}
bool InfluxDBHandler::event_mist_off(const char *reason) {
    return write_event_metric("mist_on", false, reason);
}

String InfluxDBHandler::last_error() {
    return _client->getLastErrorMessage();
}
