#include <TempHumiditySensor.h>

extern Logger *LOGGER;

TempHumiditySensor::TempHumiditySensor(uint8_t pin) {
    LOGGER->log("Initializing SensorControl");
    _sensor = new DHT(pin, DHT_TYPE);
    _sensor->begin();
}

float TempHumiditySensor::current_temperature() {
    float t = _sensor->readTemperature(USE_FAHRENHEIT);
    if (std::isnan(t)) {
        LOGGER->log_error("Failed to read temperature from LEAD sensor, using last valid temperature");
        _last_temperature_read_valid = false;
        // Use the last valid temperature if the read failed
        t = _last_temperature;
    } else {
        _last_temperature_read_valid = true;
        _last_temperature = t;
    }

    return t;
}

float TempHumiditySensor::current_humidity() {
    float h = _sensor->readHumidity();
    if (std::isnan(h)) {
        LOGGER->log_error("Failed to read humidity from LEAD sensor, using last valid humidity");
        _last_humidity_read_valid = false;
        // Use the last valid humidity if the read failed
        h = _last_humidity;
    } else {
        _last_humidity_read_valid = true;
        _last_humidity = h;
    }
    return h;
}