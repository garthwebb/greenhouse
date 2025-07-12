#ifndef INFLUXDBHANDLER_H
#define INFLUXDBHANDLER_H

#include <Arduino.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "Logger.h"

class InfluxDBHandler {
    private:
    InfluxDBClient *_client;

    const char *_device;

    bool _write_metric(Point *point);

    public:
    InfluxDBHandler(const String &serverUrl, const String &db, const char *device);

    bool write_sensor_metric(const char *sensor_id, const String &measurement, float value);
    bool write_event_metric(const String &event_type, bool state, const char *reason);
    
    bool event_fan_on(const char *reason);
    bool event_fan_off(const char *reason);
    bool event_window_open(const char *reason);
    bool event_window_closed(const char *reason);
    bool event_mist_on(const char *reason);
    bool event_mist_off(const char *reason);

    String last_error();
};

#endif