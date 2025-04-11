#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <esp_system.h>

#include "Logger.h"

class Telemetry {
    private:
    InfluxDBClient *client;
    bool log_events = true;

    public:
    Telemetry(const String &serverUrl, const String &db);

    bool report();
	String chip_model_to_string(int model);

    void enable();
    void disable();
};

#endif