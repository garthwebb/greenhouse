#include <AdminAccess.h>

extern Logger *LOGGER;

AdminAccess::AdminAccess(FanControl *fan, WindowControl *window, ClimateControl *climate)
     : _fan(fan), _window(window), _climate(climate) {
    LOGGER->log("Initializing AdminAccess");
    server = new AsyncWebServer(ADMIN_PORT);

    // Setup a handler for listning for commands on the WebSerial interface
    WebSerial.onMessage(
        [this](uint8_t *data, size_t len) { this->onMessage(data, len); }
    );
    WebSerial.begin(server);
    server->begin();

    LOGGER->log("AdminAccess available at: http://" + WiFi.localIP().toString() + "/webserial");
}

void AdminAccess::onMessage(uint8_t *data, size_t len) {
    std::string cmd = "";

    // Ignore the null termination character that's added by only going to len-1
    for (size_t i=0; i < len-1; i++) {
        cmd += char(data[i]);
    }

    // Ignore commands we don't have a handler for
    if (cmd_triggers.find(cmd) == cmd_triggers.end()) {
        Serial.println("\tignoring unknown command");
        LOGGER->log_error("Unknown AdminAccess command: " + String(cmd.c_str()));
        return;
    }

    cmd_triggers[cmd] = true;
}

void AdminAccess::handle_commands() {
    WebSerial.loop();

    for (const auto& pair : cmd_triggers) {
        // If the trigger is true, then run the command associated with it
        if (pair.second) {
            WebSerial.println("Handling command");
            Serial.println("Got triggered: " + String(pair.first.c_str()));
            LOGGER->log("Command run from AdminAccess: " + String(pair.first.c_str()));
            handlers[pair.first]("");
            cmd_triggers[pair.first] = false;
        }
    }
}

void AdminAccess::register_command(std::string cmd, std::function<void()> handler) {
    register_command(cmd, [handler](std::string arg) { handler(); });
}

void AdminAccess::register_command(std::string cmd, std::function<void(std::string)> handler) {
    // Don't add existing commands; if find doesn't return end of list, it means we already have this one
    if (handlers.find(cmd) != handlers.end()) {
        Serial.print("Command already registered: ");
        Serial.println(cmd.c_str());
        return;
    }

    Serial.print("Registering command: ");
    Serial.println(cmd.c_str());
    handlers[cmd] = handler;
    cmd_triggers[cmd] = false;

    WebSerial.println("Admin access available");
    return;
}

void AdminAccess::print_status() {
    char datetime_str[20];
    TimeHandler::localTimeString(datetime_str);

    WebSerial.printf("Up since %s\n", datetime_str);
    WebSerial.printf("- temp: %0.1f F\n", _climate->current_temperature());
    WebSerial.printf("- humidity: %0.1f%\n", _climate->current_humidity());
    WebSerial.printf("- fan is: %s\n", _fan->is_on ? "ON" : "OFF");
    WebSerial.printf("- window is: %s\n", _window->is_open ? "OPEN" : "CLOSED");
}

void AdminAccess::print_temp_history() {
    for (int i = 0; i < 8; i++) {
        WebSerial.print("history[");
        WebSerial.print(i);
        WebSerial.print("] = ");
        WebSerial.println((float) _climate->historical_temperature(i));
        WebSerial.flush();
    }
}

void AdminAccess::print_delta() {
    WebSerial.print("Rising temp delta (");
    WebSerial.print(RAPID_RISE_TEMP_MIN);
    WebSerial.print("min): ");
    WebSerial.println(_climate->rise_delta());

    WebSerial.print("Falling temp delta (");
    WebSerial.print(FALLING_TEMP_MIN);
    WebSerial.print("min): ");
    WebSerial.println(_climate->fall_delta());
}
