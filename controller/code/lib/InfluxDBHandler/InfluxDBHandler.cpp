#include <InfluxDBHandler.h>

InfluxDBHandler::InfluxDBHandler(const String &url, const String &db, const char *device, const char *ssid) {
    Serial.println("Initialziing InfluxDB: ");
 
    sensor = new Point("weather");
    window_event = new Point("window_events");
    fan_event = new Point("fan_events");

    client = new InfluxDBClient(url, db);

    // Setup tags to send with influx datapoints
    sensor->addTag("device", device);
    sensor->addTag("SSID", ssid);

    if (client->validateConnection()) {
        Serial.print("\tConnected to InfluxDB: ");
        Serial.println(client->getServerUrl());
    } else {
        Serial.print("\tInfluxDB connection failed: ");
        Serial.println(client->getLastErrorMessage());
    }
}

bool InfluxDBHandler::add_reading(float temp, float humidity) {
    // Clear fields for reusing the point. Tags will remain untouched
    sensor->clearFields();

    // Store temp & humidity
    sensor->addField("temperature",temp);
    sensor->addField("humidity", humidity);

    if (!log_events) {
        return true;
    }

    if (!client->writePoint(*sensor)) {
        return false;
    }

    return true;
}

bool InfluxDBHandler::annotate(Point *point, const char *reason, float trigger) {
    point->addField("reason", reason);
    point->addField("trigger", trigger);
  
    if (!log_events) {
        return true;
    }

    if (!client->writePoint(*point)) {
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
    log_events = true;
}

void InfluxDBHandler::disable_logging() {
    log_events = false;
}