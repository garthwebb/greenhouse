#ifndef MISTCONTROL_H
#define MISTCONTROL_H

#include <Arduino.h>

#include "Logger.h"

class MistControl {
    private:
    uint8_t _control_pin;
    bool _is_on = false;

    public:
    MistControl(uint8_t pin);
    void turn_on();
    void turn_off();

    bool is_on() const;
    bool is_off() const;
};

#endif