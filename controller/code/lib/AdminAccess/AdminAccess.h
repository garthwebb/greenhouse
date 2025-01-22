#ifndef ADMINACCESS_H
#define ADMINACCESS_H

#include <Arduino.h>
#include <WebSerial.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include <string>
#include <functional>
#include <string>

#include "Logger.h"
#include "FanControl.h"
#include "WindowControl.h"
#include "ClimateControl.h"
#include "TimeHandler.h"

#define ADMIN_PORT 80

class AdminAccess {
    private:
    AsyncWebServer *server;
    std::map<std::string, bool> cmd_triggers;
    std::map<std::string, std::function<void(std::string)>> handlers;

    FanControl *_fan;
    WindowControl *_window;
    ClimateControl *_climate;

    public:
    AdminAccess(FanControl *fan, WindowControl *window, ClimateControl *_climate);
    void onMessage(uint8_t *data, size_t len);
    void handle_commands();
    void register_command(std::string cmd,  std::function<void()>);
    void register_command(std::string cmd,  std::function<void(std::string)>);

    void print_status();
    void print_temp_history();
    void print_delta();
};

#endif