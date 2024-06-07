#ifndef INFLUXDBHANDLER_H
#define INFLUXDBHANDLER_H

#include "Arduino.h"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

class InfluxDBHandler {
    private:
    InfluxDBClient *client;
    bool log_events = true;

    Point *sensor;
    Point *window_event;
    Point *fan_event;

    bool annotate(Point *point, const char *reason, float trigger);

    public:
    InfluxDBHandler(const String &serverUrl, const String &db, const char *device, const char *ssid);

    bool add_reading(float temp, float humidity);
    bool annotate_fan_on(const char *reason, float trigger);
    bool annotate_fan_off(const char *reason, float trigger);
    bool annotate_window_open(const char *reason, float trigger);
    bool annotate_window_closed(const char *reason, float trigger);
    String last_error();
    void enable_logging();
    void disable_logging();
};

#endif