#ifndef MISTCONTROL_H
#define MISTCONTROL_H

#include <Arduino.h>

#include "Logger.h"

class MistControl {
    private:
    uint8_t control_pin;

    public:
    bool is_on = false;

    MistControl(uint8_t pin);
    void turn_on();
    void turn_off();
};

#endif