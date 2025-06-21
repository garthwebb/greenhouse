#include <MistControl.h>

extern Logger *LOGGER;

MistControl::MistControl(uint8_t pin) {
    LOGGER->log("Initializing MistControl");
    control_pin = pin;
    pinMode(control_pin, OUTPUT);
    turn_off();
}

void MistControl::turn_on() {
    digitalWrite(control_pin, HIGH);
    is_on = true;
    LOGGER->log("Misters turned on");
}

void MistControl::turn_off() {
    digitalWrite(control_pin, LOW);
    is_on = false;
    LOGGER->log("Misters turned off");
}
