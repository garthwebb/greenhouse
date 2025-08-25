#include <TempHumiditySensor.h>

extern Logger *LOGGER;

TempHumiditySensor::TempHumiditySensor() {
    LOGGER->log("Initializing SensorControl");
    _aht.begin();
}

void TempHumiditySensor::clear_cache() {
    _sensor_read = false; 
}

void TempHumiditySensor::read_sensor() {
    sensors_event_t humidity, temp;

    // populate temp and humidity objects with fresh data
    _aht.getEvent(&humidity, &temp);

    _last_humidity = humidity.relative_humidity;
    _last_pressure = humidity.pressure;
    _last_temperature = (temp.temperature * (9.0/5.0)) + 32.0;
    _sensor_read = true;    
}

float TempHumiditySensor::current_temperature() {
    // If we've already read the temperature this loop, return the cached value
    if (_sensor_read) {
        return _last_temperature;
    }

    read_sensor();

    return _last_temperature;
}

float TempHumiditySensor::current_humidity() {
    // If we've already read the humidity this loop, return the cached value
    if (_sensor_read) {
        return _last_humidity;
    }

    read_sensor();

    return _last_humidity;
}

float TempHumiditySensor::current_pressure() {
    // If we've already read the pressure this loop, return the cached value
    if (_sensor_read) {
        return _last_pressure;
    }

    read_sensor();

    return _last_pressure;
}
