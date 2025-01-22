#include <FanControl.h>

extern Logger *LOGGER;

FanControl::FanControl(uint8_t pin) {
    LOGGER->log("Initializing FanControl");
    control_pin = pin;
    pinMode(control_pin, OUTPUT);
    turn_off();
}

void FanControl::turn_on() {
    digitalWrite(control_pin, HIGH);
    is_on = true;
    LOGGER->log("Fan turned on");
}

void FanControl::turn_off() {
    digitalWrite(control_pin, LOW);
    is_on = false;
    LOGGER->log("Fan turned off");
}
