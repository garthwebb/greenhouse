#ifndef SENSORCONTROL_H
#define SENSORCONTROL_H

#include <Arduino.h>
#include <DHT.h>

#include "Logger.h"

#define DHT_TYPE DHT22
#define USE_FAHRENHEIT true

class TempHumiditySensor {
    private:
    DHT *sensor;

    public:

    TempHumiditySensor(uint8_t pin);
    float current_temperature();
    float current_humidity();
};

#endif