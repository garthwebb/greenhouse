#ifndef SENSORCONTROL_H
#define SENSORCONTROL_H

#include <Arduino.h>
#include <DHT.h>

#include "Logger.h"

#define DHT_TYPE DHT22
#define USE_FAHRENHEIT true

class TempHumiditySensor {
    private:
    DHT *_sensor;
    bool _last_humidity_read_valid = false;
    bool _last_temperature_read_valid = false;

    float _last_temperature = 0;
    float _last_humidity = 0;

    public:

    TempHumiditySensor(uint8_t pin);
    float current_temperature();
    float current_humidity();


};

#endif