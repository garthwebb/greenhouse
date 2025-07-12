#include <WindowControl.h>

extern Logger *LOGGER;

WindowControl::WindowControl(uint8_t open_pin, uint8_t close_pin)
    : _control_pin_open(open_pin), _control_pin_close(close_pin) {
    LOGGER->log("Initializing WindowControl");

    pinMode(_control_pin_open, OUTPUT);
    digitalWrite(_control_pin_open, LOW);

    pinMode(_control_pin_close, OUTPUT);
    digitalWrite(_control_pin_close, LOW);

    // There is no sensor to tell whether the window is open or not, so
    // send the close command regardless just to get to a known state. 
    Serial.print("Closing window ... ");
    close();
    Serial.println("done");
}

void WindowControl::monitor() {
    // If the window isn't in the process of opening or closing, exit
    if (is_stopped()) {
        return;
    }

    // If WINDOW_MOVE_TIME_MS has elapsed, then set all control pins to low to stop everything
    if (millis() >= move_start_ms + WINDOW_MOVE_TIME_MS) {
        digitalWrite(_control_pin_open, LOW);
        digitalWrite(_control_pin_close, LOW);
        _is_moving = false;

        Serial.println("<< Window has finished moving >>");
        LOGGER->log("Window has finished moving");
    }
}

void WindowControl::open() {
    // Safeguard against open being called while its already trying to close; shouldn't happen!
    digitalWrite(_control_pin_close, LOW);

    digitalWrite(_control_pin_open, HIGH);
    move_start_ms = millis();
    _is_moving = true;

    _is_open = true;
    last_open_time_ms = millis();

    LOGGER->log("Window open started");
}

void WindowControl::close() {
    // Safeguard against close being called while its already trying to open; shouldn't happen!
    digitalWrite(_control_pin_open, LOW);

    digitalWrite(_control_pin_close, HIGH);
    move_start_ms = millis();
    _is_moving = true;

    _is_open = false;

    LOGGER->log("Window close started");
}

bool WindowControl::is_open() {
    return _is_open;
}

bool WindowControl::is_closed() {
    return !_is_open;
}

bool WindowControl::is_moving() {
    return _is_moving;
}

bool WindowControl::is_stopped() {
    return !_is_moving;
}