#ifndef FANCONTROL_H
#define FANCONTROL_H

#include <Arduino.h>

#include "Logger.h"

//Point fan_state("fan_events");

class FanControl {
    private:
    uint8_t control_pin;

    public:
    bool is_on = false;

    FanControl(uint8_t pin);
    void turn_on();
    void turn_off();
};

#endif