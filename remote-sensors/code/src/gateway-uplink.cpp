// Gateway uplink: reads sensor lines from the gateway radio over UART and publishes each reading to
// MQTT on tiger-pi.  This board is an ordinary WiFi client and can roam between APs freely.

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <time.h>
#include <map>
#include <string>

#include <Logger.h>
#include "WirelessControl.h"

#include "wifi-info.h"
#include "remote_sensor_protocol.h"

//----------------------------------------------------
// Defines

#ifndef HOSTNAME
#define HOSTNAME "greenhouse-sensor-gateway"
#endif

#ifndef RADIO_TX_PIN
#define RADIO_TX_PIN D6
#endif
#ifndef RADIO_RX_PIN
#define RADIO_RX_PIN D7
#endif

// Syslog server connection info
#define SYSLOG_SERVER "tigerbackup.local"
#define SYSLOG_PORT 514
#define APP_NAME "sensor-gateway"

// MQTT broker on tiger-pi
#ifndef MQTT_HOST
#define MQTT_HOST "192.168.3.88"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#define MQTT_SENSOR_TOPIC_PREFIX "greenhouse/sensors/"
#define MQTT_STATUS_TOPIC "greenhouse/gateway/status"
#define MQTT_BUFFER_SIZE 512
#define MQTT_RECONNECT_PERIOD_MS 5000

// Epoch seconds before which we assume NTP hasn't synced yet
#define MIN_VALID_EPOCH 1700000000

// Send a heartbeat every 60 minutes to the logger
#define HEARTBEAT_PERIOD_MS (60 * 60 * 1000)

#define WDT_TIMEOUT_S 120

//----------------------------------------------------
// Globals

Logger *LOGGER = nullptr;

WiFiClient wifi_client;
PubSubClient mqtt(wifi_client);

char line_buffer[UART_LINK_MAX_LINE];
size_t line_length = 0;

// Last sequence number published per sensor, to drop resends whose ACK was lost
std::map<std::string, uint32_t> last_seq;

uint32_t published_count = 0;
uint32_t duplicate_count = 0;
uint32_t dropped_count = 0;
uint32_t bad_line_count = 0;

// Make sure we try to connect immediately
unsigned long last_mqtt_attempt_ms = millis() - MQTT_RECONNECT_PERIOD_MS;
unsigned long last_heartbeat_ms = millis();

//----------------------------------------------------
// Functions

void monitor_mqtt() {
    if (mqtt.connected()) {
        mqtt.loop();
        return;
    }

    if (millis() - last_mqtt_attempt_ms < MQTT_RECONNECT_PERIOD_MS) {
        return;
    }
    last_mqtt_attempt_ms = millis();

    // The broker publishes "offline" for us if we drop off without saying goodbye
    if (mqtt.connect(HOSTNAME, nullptr, nullptr, MQTT_STATUS_TOPIC, 0, true, "offline")) {
        mqtt.publish(MQTT_STATUS_TOPIC, "online", true);
        LOGGER->log("Connected to MQTT broker at " + String(MQTT_HOST) + ":" + String(MQTT_PORT));
    } else {
        LOGGER->log_error("MQTT connection failed, state " + String(mqtt.state()));
    }
}

void publish_reading(const char *id, uint32_t seq, int temp_centi_c, unsigned int humidity_centi,
                     unsigned int battery_mv, int rssi, unsigned int attempt, unsigned int missed,
                     unsigned int battery_used_mah, unsigned int battery_capacity_mah) {
    auto previous = last_seq.find(id);
    if (previous != last_seq.end() && previous->second == seq) {
        duplicate_count++;
        return;
    }

    time_t now = time(nullptr);
    if (now < MIN_VALID_EPOCH) {
        LOGGER->log_warn("Dropping reading from " + String(id) + ": no NTP time yet");
        dropped_count++;
        return;
    }

    if (!mqtt.connected()) {
        LOGGER->log_warn("Dropping reading from " + String(id) + ": MQTT not connected");
        dropped_count++;
        return;
    }

    // Temperature in F to match the controller and the existing "weather" series in InfluxDB
    float temp_f = (temp_centi_c / 100.0f) * (9.0f / 5.0f) + 32.0f;

    JsonDocument doc;
    doc["id"] = id;
    doc["temperature"] = serialized(String(temp_f, 2));
    doc["humidity"] = serialized(String(humidity_centi / 100.0f, 2));
    if (battery_mv > 0) {
        doc["battery_v"] = serialized(String(battery_mv / 1000.0f, 3));
    }
    if (battery_capacity_mah > 0) {
        float remaining = 100.0f * (1.0f - (float)battery_used_mah / battery_capacity_mah);
        doc["battery_used_mah"] = battery_used_mah;
        doc["battery_pct"] = serialized(String(constrain(remaining, 0.0f, 100.0f), 1));
    }
    doc["rssi"] = rssi;
    doc["seq"] = seq;
    doc["attempt"] = attempt;
    doc["missed"] = missed;
    doc["ts"] = (uint32_t)now;

    char payload[MQTT_BUFFER_SIZE];
    serializeJson(doc, payload, sizeof(payload));

    // Retained so a subscriber that (re)connects gets the latest reading from every sensor right away
    String topic = String(MQTT_SENSOR_TOPIC_PREFIX) + id + "/state";
    if (!mqtt.publish(topic.c_str(), payload, true)) {
        LOGGER->log_error("Failed to publish reading from " + String(id));
        dropped_count++;
        return;
    }

    last_seq[id] = seq;
    published_count++;
    Serial.printf("%s %s\n", topic.c_str(), payload);
}

void handle_line(const char *line) {
    const char *star = strrchr(line, '*');
    if (line[0] != '$' || star == nullptr) {
        bad_line_count++;
        return;
    }

    unsigned int expected_checksum;
    if (sscanf(star + 1, "%2x", &expected_checksum) != 1 ||
        uart_link_checksum(line + 1, star - line - 1) != expected_checksum) {
        bad_line_count++;
        return;
    }

    char id[13];
    unsigned int seq, humidity_centi, battery_mv, attempt, missed, battery_used_mah, battery_capacity_mah;
    int temp_centi_c, rssi;
    int fields = sscanf(line, "$GH,%12[0-9a-f],%u,%d,%u,%u,%d,%u,%u,%u,%u*",
                        id, &seq, &temp_centi_c, &humidity_centi, &battery_mv, &rssi, &attempt, &missed,
                        &battery_used_mah, &battery_capacity_mah);
    if (fields != 10) {
        bad_line_count++;
        return;
    }

    publish_reading(id, seq, temp_centi_c, humidity_centi, battery_mv, rssi, attempt, missed,
                    battery_used_mah, battery_capacity_mah);
}

void read_radio() {
    while (Serial1.available()) {
        char c = Serial1.read();

        if (c == '$') {
            // Start of a line; also resyncs us after any garbage
            line_length = 0;
        }

        if (c == '\n') {
            line_buffer[line_length] = '\0';
            if (line_length > 0) {
                handle_line(line_buffer);
            }
            line_length = 0;
        } else if (c != '\r' && line_length < sizeof(line_buffer) - 1) {
            line_buffer[line_length++] = c;
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(UART_LINK_BAUD, SERIAL_8N1, RADIO_RX_PIN, RADIO_TX_PIN);

    // Initialize the logger so WirelessControl can use it, but LOGGER should not be used
    // until after the init_wifi() returns
    LOGGER = new Logger();
    LOGGER->init(SYSLOG_SERVER, SYSLOG_PORT, HOSTNAME, APP_NAME);

    WirelessControl::init_wifi(WIFI_SSID, WIFI_PASSWORD, HOSTNAME);
    LOGGER->log("Sensor gateway uplink power cycled, starting up ...");

    // UTC is all we need; the timestamp goes into InfluxDB as epoch seconds
    configTime(0, 0, "pool.ntp.org", "time.nis.gov");

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(MQTT_BUFFER_SIZE);

    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);
}

void loop() {
    // Make sure we still have a wifi connection
    WirelessControl::monitor();

    monitor_mqtt();
    read_radio();

    if (millis() - last_heartbeat_ms >= HEARTBEAT_PERIOD_MS) {
        LOGGER->log("Sensor gateway running: published=" + String(published_count) +
                    " duplicates=" + String(duplicate_count) + " dropped=" + String(dropped_count) +
                    " bad_lines=" + String(bad_line_count));
        last_heartbeat_ms = millis();
    }

    esp_task_wdt_reset();
}
