#ifndef REMOTESENSORS_H
#define REMOTESENSORS_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <map>
#include <string>

#include "Logger.h"

#define REMOTE_SENSOR_TOPIC "greenhouse/sensors/+/state"

// Readings older than this are considered stale, e.g. a sensor with a dead battery
#define REMOTE_SENSOR_STALE_S (15 * 60)

// A sensor's battery is low when either its estimated charge or its voltage under load drops below these.
// The voltage check catches a battery that's running out faster than the charge estimate thinks.
#define REMOTE_LOW_BATTERY_PCT 15.0
#define REMOTE_LOW_BATTERY_V 2.6

#define MQTT_RECONNECT_PERIOD_MS 30000
#define MQTT_BUFFER_SIZE 512

struct RemoteReading {
    float temperature = NAN;    // F
    float humidity = NAN;
    float battery_v = NAN;      // under load; NAN when the sensor doesn't report it
    float battery_pct = NAN;    // estimated charge remaining; NAN when the sensor doesn't report it
    bool low_battery = false;
    int rssi = 0;
    uint32_t missed = 0;
    time_t ts = 0;              // epoch seconds the gateway received the reading
};

class RemoteSensors {
    private:
    WiFiClient _wifi_client;
    PubSubClient _mqtt;
    String _client_id;
    unsigned long _last_connect_attempt_ms;

    std::map<std::string, RemoteReading> _readings;

    void _connect();
    void _on_message(char *topic, uint8_t *payload, unsigned int length);

    public:
    RemoteSensors(const char *host, uint16_t port, const char *client_id);

    // Call every loop; reconnects to the broker as needed and processes incoming readings
    void monitor();
    bool is_connected();

    // Latest reading from each sensor seen, keyed by the sensor's MAC
    const std::map<std::string, RemoteReading>& readings() const;

    bool is_fresh(const RemoteReading &reading) const;
    long age_s(const RemoteReading &reading) const;
};

#endif
