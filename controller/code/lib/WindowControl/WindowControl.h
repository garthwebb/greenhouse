#ifndef WINDOWCONTROL_H
#define WINDOWCONTROL_H

#include <Arduino.h>

#include "Logger.h"

// Time in seconds to wait for window to close
#define WINDOW_MOVE_TIME_S 20
#define WINDOW_MOVE_TIME_MS (1000 * WINDOW_MOVE_TIME_S)

class WindowControl {
    private:
    uint8_t _control_pin_open;
    uint8_t _control_pin_close;
    bool _is_open = false;
    bool _is_moving = false;

    public:
    long move_start_ms = 0;
    long last_open_time_ms = 0;


    WindowControl(uint8_t open_pin, uint8_t close_pin);
    void open();
    void close();
    void monitor();
    long millis_since_open();
    long seconds_since_open();

    bool is_open();
    bool is_closed();
    bool is_moving();
    bool is_stopped();
};

#endif