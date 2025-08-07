#ifndef SENSORCONTROL_H
#define SENSORCONTROL_H

#include <Arduino.h>
#include <DHT.h>

#include "Logger.h"

#define DHT_TYPE DHT22
#define USE_FAHRENHEIT true
#define SENSOR_TIMEOUT_MS 10000
#define SENSOR_TIMEOUT_CHECK_MS 100

class TempHumiditySensor {
    private:
    DHT *_sensor;
    bool _last_humidity_read_valid = false;
    bool _last_temperature_read_valid = false;

    float _last_temperature = 0;
    float _last_humidity = 0;

    bool _temp_read_this_loop = false;
    bool _humidity_read_this_loop = false;
    bool _sensor_read_done = false;

    public:

    TempHumiditySensor(uint8_t pin);
    static void read_temperature_task(void* param);
    static void read_humidity_task(void* param);

    void read_temperature();
    void read_humidity();

    float current_temperature();
    float current_humidity();

    void clear_cache();

};

#endif