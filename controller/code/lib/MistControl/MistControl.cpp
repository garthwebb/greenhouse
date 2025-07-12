#include <MistControl.h>

extern Logger *LOGGER;

MistControl::MistControl(uint8_t pin) : _control_pin(pin) {
    LOGGER->log("Initializing MistControl");
    pinMode(_control_pin, OUTPUT);
    turn_off();
}

void MistControl::turn_on() {
    digitalWrite(_control_pin, HIGH);
    _is_on = true;
    LOGGER->log("Misters turned on");
}

void MistControl::turn_off() {
    digitalWrite(_control_pin, LOW);
    _is_on = false;
    LOGGER->log("Misters turned off");
}

bool MistControl::is_on() const {
    return _is_on;
}

bool MistControl::is_off() const {
    return !_is_on;
}