#include <SensorControl.h>

extern Logger *LOGGER;

SensorControl::SensorControl(uint8_t pin) {
    LOGGER->log("Initializing SensorControl");
    sensor = new DHT(pin, DHT_TYPE);
    sensor->begin();
}

float SensorControl::current_temperature() {
    float t = sensor->readTemperature(USE_FAHRENHEIT);
    return std::isnan(t) ? 0 : t;
}

float SensorControl::current_humidity() {
    float h = sensor->readHumidity();
    return std::isnan(h) ? 0 : h;
}