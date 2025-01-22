#ifndef TIMEHANDLER_H
#define TIMEHANDLER_H

#include <Arduino.h>

// From https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
#define TIME_ZONE "PST8PDT,M3.2.0,M11.1.0"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nis.gov"

class TimeHandler {
    private:

    public:

    static void init_ntp();
    static void localTimeString(char *datetime);
};

#endif