#include <FanControl.h>

FanControl::FanControl(uint8_t pin) {
    control_pin = pin;
    pinMode(control_pin, OUTPUT);
    turn_off();
}

void FanControl::turn_on() {
    digitalWrite(control_pin, HIGH);
    is_on = true;
}

void FanControl::turn_off() {
    digitalWrite(control_pin, LOW);
    is_on = false;
}
