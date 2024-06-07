#include <WindowControl.h>

WindowControl::WindowControl(uint8_t open_pin, uint8_t close_pin) {
    control_pin_open = open_pin;
    control_pin_close = close_pin;

    pinMode(control_pin_open, OUTPUT);
    digitalWrite(control_pin_open, LOW);

    pinMode(control_pin_close, OUTPUT);
    digitalWrite(control_pin_close, LOW);

    // There is no sensor to tell whether the window is open or not, so
    // send the close command regardless just to get to a known state. 
    Serial.print("Closing window ... ");
    close();
    Serial.println("done");
}

void WindowControl::monitor() {
    // If the window isn't in the process of opening or closing, exit
    if (!is_moving) {
        return;
    }

    // If WINDOW_MOVE_TIME_MS has elapsed, then set all control pins to low to stop everything
    if (millis() >= move_start_ms + WINDOW_MOVE_TIME_MS) {
        digitalWrite(control_pin_open, LOW);
        digitalWrite(control_pin_close, LOW);
        is_moving = false;

        Serial.println("<< Window has finished moving >>");
    }
}

void WindowControl::open() {
    // Safeguard against open being called while its already trying to close; shouldn't happen!
    digitalWrite(control_pin_close, LOW);

    digitalWrite(control_pin_open, HIGH);
    move_start_ms = millis();
    is_moving = true;

    is_open = true;
    last_open_time_ms = millis();
}

void WindowControl::close() {
    // Safeguard against close being called while its already trying to open; shouldn't happen!
    digitalWrite(control_pin_open, LOW);

    digitalWrite(control_pin_close, HIGH);
    move_start_ms = millis();
    is_moving = true;

    is_open = false;
}