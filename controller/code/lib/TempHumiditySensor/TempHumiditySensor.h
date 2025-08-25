#ifndef SENSORCONTROL_H
#define SENSORCONTROL_H

#include <Arduino.h>

#include "Logger.h"
#include <Adafruit_AHTX0.h>

#define DHT_TYPE DHT22
#define USE_FAHRENHEIT true
#define SENSOR_TIMEOUT_MS 10000
#define SENSOR_TIMEOUT_CHECK_MS 100

class TempHumiditySensor {
    private:
    Adafruit_AHTX0 _aht;

    float _last_temperature = 0;
    float _last_humidity = 0;
    float _last_pressure = 0;

    bool _sensor_read = false;

    public:

    TempHumiditySensor();

    void read_sensor();

    float current_temperature();
    float current_humidity();
    float current_pressure();

    void clear_cache();
};

#endif