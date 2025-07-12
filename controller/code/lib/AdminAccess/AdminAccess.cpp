#include <AdminAccess.h>

extern Logger *LOGGER;

AdminAccess::AdminAccess(ExternalSettings *settings, ControlObjects *controls, SensorObjects *sensors, ClimateControl *climate)
     : _settings(settings), _controls(controls), _sensors(sensors), _climate(climate) {
    LOGGER->log("Initializing AdminAccess");
    server = new AsyncWebServer(ADMIN_PORT);

    // Setup a handler for listning for commands on the WebSerial interface
    WebSerial.onMessage(
        [this](uint8_t *data, size_t len) { this->onMessage(data, len); }
    );
    WebSerial.begin(server);
    server->begin();

    LOGGER->log("AdminAccess available at: http://" + WiFi.localIP().toString() + "/webserial");
    Serial.println("AdminAccess available at: http://" + WiFi.localIP().toString() + "/webserial");
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

        WebSerial.println("Command not found.");
        print_help();
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

void AdminAccess::print_help() {
    String commands = "Available commands:\n";
    for (const auto& pair : handlers) {
        commands += "- " + String(pair.first.c_str()) + "\n";
    }
    WebSerial.println(commands);
}

void AdminAccess::print_status() {
    char datetime_str[20];
    TimeHandler::localTimeString(datetime_str);

    WebSerial.printf("Up since %s\n", datetime_str);
	WebSerial.println("Current Readings:");
	Serial.println("Current Readings:");

	WebSerial.printf("LEAD Sensor: temp: %.2fF, humidity: %.2f%%\n", _sensors->temphumid->current_temperature(), _climate->current_humidity());
	Serial.printf("LEAD Sensor: temp: %.2fF, humidity: %.2f%%\n", _sensors->temphumid->current_temperature(), _sensors->temphumid->current_humidity());

	_sensors->temp->load_readings();
	for (auto &sensor : _sensors->temp->sensors) {
		WebSerial.printf("Sensor [%s]: temp: %.2fC\n", sensor.get_address_string().c_str(), sensor.temp);
		Serial.printf("Sensor [%s]: temp: %.2fC\n", sensor.get_address_string().c_str(), sensor.temp);
	}

	_sensors->light->read();
	WebSerial.printf("Light Sensor: full=%d, ir=%d, visible=%d, lux=%.2f lux\n",
						_sensors->light->getFullLuminosity(), _sensors->light->getIR(), _sensors->light->getVisible(), _sensors->light->getLux());
	Serial.printf("Light Sensor: full=%d, ir=%d, visible=%d, lux=%.2f lux\n",
						_sensors->light->getFullLuminosity(), _sensors->light->getIR(), _sensors->light->getVisible(), _sensors->light->getLux());

    WebSerial.printf("- fan is: %s\n", _controls->fan->is_on() ? "ON" : "OFF");
    WebSerial.printf("- window is: %s\n", _controls->window->is_open() ? "OPEN" : "CLOSED");
    WebSerial.printf("- mist is: %s\n", _controls->mist->is_on() ? "ON" : "OFF");
}

void AdminAccess::print_delta() {
    WebSerial.printf("Long period temp delta (%d sec): %0.2f F\n", 
                     _settings->get_temp_long_delta_s(), _climate->get_long_temp_delta());

    WebSerial.printf("Short period temp delta (%d sec): %0.2f F\n", 
                     _settings->get_temp_short_delta_s(), _climate->get_short_temp_delta());

}
