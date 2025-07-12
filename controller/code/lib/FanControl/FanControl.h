#ifndef FANCONTROL_H
#define FANCONTROL_H

#include <Arduino.h>

#include "Logger.h"

//Point fan_state("fan_events");

class FanControl {
    private:
    uint8_t _control_pin;
    bool _is_on = false;

    public:
    FanControl(uint8_t pin);

    void turn_on();
    void turn_off();

    bool is_on();
    bool is_off();
};

#endif