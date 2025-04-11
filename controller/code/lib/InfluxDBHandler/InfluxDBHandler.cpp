#include <InfluxDBHandler.h>
#include "WirelessControl.h"

extern Logger *LOGGER;

InfluxDBHandler::InfluxDBHandler(const String &url, const String &db, const char *device, const char *ssid) {
    LOGGER->log("Initializing InfluxDBHandler");
    Serial.println("Initialziing InfluxDB: ");
 
    sensor = new Point("weather");
    window_event = new Point("window_events");
    fan_event = new Point("fan_events");

	return;

    client = new InfluxDBClient(url, db);

    // Setup tags to send with influx datapoints
    sensor->addTag("device", device);
    sensor->addTag("SSID", ssid);

    if (client->validateConnection()) {
        Serial.println("\tConnected to InfluxDB: " + client->getServerUrl());
        LOGGER->log("Connected to InfluxDB at " + client->getServerUrl());
    } else {
        Serial.println("\tInfluxDB connection failed: " + client->getLastErrorMessage());
        LOGGER->log_error("InfluxDB connection failed: " + client->getLastErrorMessage());
    }

    if (log_events && WirelessControl::is_connected) {
        LOGGER->log("Logging events to InfluxDB is enabled");
    }
}

bool InfluxDBHandler::add_reading(float temp, float humidity) {
    // Clear fields for reusing the point. Tags will remain untouched
    sensor->clearFields();

    // Store temp & humidity
    sensor->addField("temperature", temp);
    sensor->addField("humidity", humidity);

    // Can we log this event?
    if (!log_events || !WirelessControl::is_connected) {
        return true;
    }

    if (!client->writePoint(*sensor)) {
        LOGGER->log_error("Failed to add reading: " + last_error());
        return false;
    }

    return true;
}

bool InfluxDBHandler::annotate(Point *point, const char *reason, float trigger) {
    point->addField("reason", reason);
    point->addField("trigger", trigger);
  
    if (!log_events || !WirelessControl::is_connected) {
        return true;
    }

    if (!client->writePoint(*point)) {
        LOGGER->log_error("Failed to add annotation: " + last_error());
        return false;
    }

    return true;
}

bool InfluxDBHandler::annotate_fan_on(const char *reason, float trigger) {
    fan_event->clearFields();
    fan_event->addField("fan_on", true);
    return annotate(fan_event, reason, trigger);
}

bool InfluxDBHandler::annotate_fan_off(const char *reason, float trigger) {
    fan_event->clearFields();
    fan_event->addField("fan_on", false);
    return annotate(fan_event, reason, trigger);
}

bool InfluxDBHandler::annotate_window_open(const char *reason, float trigger) {
    window_event->clearFields();
    window_event->addField("is_open", true);
    return annotate(window_event, reason, trigger);
}

bool InfluxDBHandler::annotate_window_closed(const char *reason, float trigger) {
    window_event->clearFields();
    window_event->addField("is_open", false);
    return annotate(window_event, reason, trigger);
}

String InfluxDBHandler::last_error() {
    return client->getLastErrorMessage();
}

void InfluxDBHandler::enable_logging() {
    LOGGER->log("Logging events to InfluxDB is enabled");
    log_events = true;
}

void InfluxDBHandler::disable_logging() {
    LOGGER->log("Logging events to InfluxDB is *disabled*");
    log_events = false;
}