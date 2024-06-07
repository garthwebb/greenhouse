#ifndef ADMINACCESS_H
#define ADMINACCESS_H

#include "Arduino.h"

#include <WebSerial.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include <string>
#include <functional>

#define ADMIN_PORT 80

class AdminAccess {
    private:
    AsyncWebServer *server;
    std::map<std::string, bool> cmd_triggers;
    std::map<std::string, std::function<void(std::string)>> handlers;

    public:
    AdminAccess();
    void onMessage(uint8_t *data, size_t len);
    void handle_commands();
    void register_command(std::string cmd,  std::function<void()>);
    void register_command(std::string cmd,  std::function<void(std::string)>);
    void run_command(std::string cmd, std::string arg);
};

#endif