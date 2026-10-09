#include <FanControl.h>

extern Logger *LOGGER;

FanControl::FanControl(uint8_t pin) : _control_pin(pin) {
        LOGGER->log("Initializing FanControl");
        pinMode(_control_pin, OUTPUT);
        turn_off();
    }

void FanControl::turn_on() {
    digitalWrite(_control_pin, HIGH);
    _is_on = true;
    _last_change_ms = millis();
    LOGGER->log("Fan turned on");
}

void FanControl::turn_off() {
    digitalWrite(_control_pin, LOW);
    _is_on = false;
    _last_change_ms = millis();
    LOGGER->log("Fan turned off");
}

bool FanControl::is_on() {
    return _is_on;
}

bool FanControl::is_off() {
    return !_is_on;
}

unsigned long FanControl::millis_since_change() {
    return millis() - _last_change_ms;
}
