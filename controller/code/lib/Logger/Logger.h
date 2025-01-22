#ifndef LOGGER_H
#define LOGGER_H

#include <Syslog.h>
#include <WiFiUdp.h>
#include <string>

#include "monitor.h"

// Syslog server connection info
#define SYSLOG_SERVER "tigerbackup.local"
//#define SYSLOG_IP IPAddress(192, 168, 3, 204)
#define SYSLOG_PORT 514
#define DEVICE_HOSTNAME "greenhouse-esp32"
#define APP_NAME "env-control"

class Logger {
	public:

	bool log(String msg);
	bool log_info(String msg);
	bool log_error(String msg);
	bool log_debug(String msg);
};

#endif