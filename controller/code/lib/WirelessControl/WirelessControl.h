#ifndef WIRELESSCONTROL_H
#define WIRELESSCONTROL_H

#include <Arduino.h>

#include "monitor.h"
#include "Logger.h"

// Wait 10s for a connection
#define WIFI_CONNECT_WAIT 5000

class WirelessControl {
    public:

    WirelessControl();
    static void init_wifi(const char *ssid, const char *passwd);
    static void listNetworks();
    static void monitor();

    static bool is_connected;

    private:
    static void printStatus(uint8_t status);
    static void printEncryptionType(int thisType);
};

#endif