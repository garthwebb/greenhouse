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
#include "ClimateControl.h"
#include "TimeHandler.h"
#include "ExternalSettings.h"

#define ADMIN_PORT 80

class AdminAccess {
    private:
    AsyncWebServer *server;
    std::map<std::string, bool> cmd_triggers;
    std::map<std::string, std::function<void(std::string)>> handlers;

    ExternalSettings *_settings;
	ControlObjects *_controls;
    SensorObjects *_sensors;
    ClimateControl *_climate;

    public:
    AdminAccess(ExternalSettings *settings, ControlObjects *controls, SensorObjects *sensors, ClimateControl *climate);
    void onMessage(uint8_t *data, size_t len);
    void handle_commands();
    void register_command(std::string cmd,  std::function<void()>);
    void register_command(std::string cmd,  std::function<void(std::string)>);

    void print_help();
    void print_status();
    void print_delta();
};

#endif