#include <TempHumiditySensor.h>

extern Logger *LOGGER;

TempHumiditySensor::TempHumiditySensor(uint8_t pin) {
    LOGGER->log("Initializing SensorControl");
    _sensor = new DHT(pin, DHT_TYPE);
    _sensor->begin();
}

void TempHumiditySensor::clear_cache() {
    _temp_read_this_loop  = false;
    _humidity_read_this_loop = false;
}

void TempHumiditySensor::read_temperature_task(void *param) {
    TempHumiditySensor* self = static_cast<TempHumiditySensor*>(param);
    self->read_temperature();
    vTaskDelete(NULL);
}

void TempHumiditySensor::read_temperature() {
    float t = _sensor->readTemperature(USE_FAHRENHEIT);

    if (std::isnan(t)) {
        LOGGER->log_error("Failed to read temperature from LEAD sensor");
    } else {
        _last_temperature = t;
    }

    _sensor_read_done = true;
}

void TempHumiditySensor::read_humidity_task(void *param) {
    TempHumiditySensor* self = static_cast<TempHumiditySensor*>(param);
    self->read_humidity();
    vTaskDelete(NULL);
}

void TempHumiditySensor::read_humidity() {
    float h = _sensor->readHumidity();

    if (std::isnan(h)) {
        LOGGER->log_error("Failed to read humidity from LEAD sensor, using last valid humidity");
    } else {
        _last_humidity = h;
    }

    _sensor_read_done = true;
}   

float TempHumiditySensor::current_temperature() {
    // If we've already read the temperature this loop, return the cached value
    if (_temp_read_this_loop) {
        return _last_temperature;
    }

    // Create a task to read the temperature so that we can exit if it hangs
    _sensor_read_done = false;
    xTaskCreate(read_temperature_task, "sensorTask", 2048, this, 1, NULL);

    unsigned long start = millis();
    while (!_sensor_read_done && (millis() - start) < SENSOR_TIMEOUT_MS) {
        delay(SENSOR_TIMEOUT_CHECK_MS);
    }

    if (!_sensor_read_done) {
        LOGGER->log_error("LEAD Sensor temperature read timed out");
    }

    _temp_read_this_loop = true;
    return _last_temperature;
}

float TempHumiditySensor::current_humidity() {
    // If we've already read the humidity this loop, return the cached value
    if (_humidity_read_this_loop) {
        return _last_humidity;
    }

    // Create a task to read the humidity so that we can exit if it hangs
    _sensor_read_done = false;
    xTaskCreate(read_humidity_task, "sensorTask", 2048, this, 1, NULL);

    unsigned long start = millis();
    while (!_sensor_read_done && (millis() - start) < SENSOR_TIMEOUT_MS) {
        delay(SENSOR_TIMEOUT_CHECK_MS);
    }

    if (!_sensor_read_done) {
        LOGGER->log_error("LEAD Sensor humidity read timed out");
    }

    _temp_read_this_loop = true;
    return _last_humidity;
}