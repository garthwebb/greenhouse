#include <AdminAccess.h>

AdminAccess::AdminAccess() {
    server = new AsyncWebServer(ADMIN_PORT);

    // Setup a handler for listning for commands on the WebSerial interface
    WebSerial.onMessage(
        [this](uint8_t *data, size_t len) { this->onMessage(data, len); }
    );
    WebSerial.begin(server);
    server->begin();
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
        return;
    }

    cmd_triggers[cmd] = true;
}

void AdminAccess::handle_commands() {
    for (const auto& pair : cmd_triggers) {
        // If the trigger is true, then run the command associated with it
        if (pair.second) {
            WebSerial.println("Handling command");
            Serial.print("Got triggered: ");
            Serial.println(pair.first.c_str());
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

void AdminAccess::run_command(std::string cmd, std::string arg) {
    if (handlers.find(cmd) == handlers.end()) {
        return;
    }

    handlers[cmd](arg);
}