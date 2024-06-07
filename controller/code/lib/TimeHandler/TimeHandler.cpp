#include <TimeHandler.h>

void TimeHandler::init_ntp() {
  // Accurate time is necessary for certificate validation and writing in batches
  //timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  Serial.print("Syncing time with NTP ... ");
  configTzTime(TIME_ZONE, NTP_SERVER_1, NTP_SERVER_2);
  Serial.println("done");
}

void TimeHandler::localTimeString(char* datetime) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("No time available (yet)");
        return;
    }

    int hour = timeinfo.tm_hour;
    int min = timeinfo.tm_min;
    int sec = timeinfo.tm_sec;

    int day = timeinfo.tm_mday;
    int month = timeinfo.tm_mon + 1;
    int year = timeinfo.tm_year +1900;

    sprintf(datetime, "%4d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, min, sec);
}