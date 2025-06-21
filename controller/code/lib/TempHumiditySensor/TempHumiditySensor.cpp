#include <TempHumiditySensor.h>

extern Logger *LOGGER;

TempHumiditySensor::TempHumiditySensor(uint8_t pin) {
    LOGGER->log("Initializing SensorControl");
    sensor = new DHT(pin, DHT_TYPE);
    sensor->begin();
}

float TempHumiditySensor::current_temperature() {
    float t = sensor->readTemperature(USE_FAHRENHEIT);
    return std::isnan(t) ? 0 : t;
}

float TempHumiditySensor::current_humidity() {
    float h = sensor->readHumidity();
    return std::isnan(h) ? 0 : h;
}