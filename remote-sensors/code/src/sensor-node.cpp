// Battery powered temp/humidity sensor.  Wakes from deep sleep, reads an SHT4x, sends one ESP-NOW
// packet to the gateway radio and goes back to sleep.  Awake time is ~100ms per reading.

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_SHT4x.h>

#include "remote_sensor_protocol.h"

//----------------------------------------------------
// Defines

// Time in seconds between readings
#ifndef SLEEP_S
#define SLEEP_S (5 * 60)
#endif

// How many times to try a send before giving up until the next wake
#ifndef SEND_ATTEMPTS
#define SEND_ATTEMPTS 3
#endif
#define SEND_ACK_TIMEOUT_MS 50
#define SEND_RETRY_DELAY_MS 20

// Ratio of the battery voltage divider, e.g. 2.0 for two equal resistors
#ifndef BATTERY_DIVIDER
#define BATTERY_DIVIDER 2.0
#endif
#define BATTERY_SAMPLES 8

// With BATTERY_ENABLE_PIN defined, the divider is switched in by a MOSFET only while reading, so it
// draws nothing during sleep.  While it's off the ADC filter cap sits at the full battery voltage, and
// has to discharge to half through the divider's 50k (100k || 100k).  With 10nF that's a 0.5ms time
// constant; 5ms is 10 of them, settling to well under 1mV.  Scale this with the cap if it changes.
#ifndef BATTERY_SETTLE_MS
#define BATTERY_SETTLE_MS 5
#endif

// Usable capacity down to where the boost converter can no longer hold 3.3V
#ifndef BATTERY_CAPACITY_MAH
#define BATTERY_CAPACITY_MAH 1300
#endif

// Charge counting estimates.  These are battery-side currents, so they include the boost converter's
// losses; calibrate them by measuring the battery current with a power profiler.
#ifndef SLEEP_CURRENT_UA
#define SLEEP_CURRENT_UA 50.0
#endif
#ifndef AWAKE_CURRENT_MA
#define AWAKE_CURRENT_MA 60.0
#endif
// Time from wake to setup() that millis() doesn't see
#ifndef BOOT_OVERHEAD_MS
#define BOOT_OVERHEAD_MS 30
#endif

#define CHARGE_MAGIC 0x43484731

// Above this humidity, pulse the SHT4x heater after reading to dry off any condensation
#define HEATER_HUMIDITY_THRESHOLD 95.0

#ifdef SENSOR_DEBUG
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

//----------------------------------------------------
// Globals

// Survive deep sleep
RTC_DATA_ATTR uint32_t seq = 0;
RTC_DATA_ATTR uint8_t missed = 0;

// Survive deep sleep and any reset short of losing power, i.e. everything but a battery change
RTC_NOINIT_ATTR uint32_t charge_magic;
RTC_NOINIT_ATTR double charge_used_mah;

volatile bool send_done = false;
volatile bool send_ok = false;

const uint8_t gateway_mac[] = GATEWAY_RADIO_MAC;

//----------------------------------------------------
// Functions

void on_sent(const uint8_t *mac, esp_now_send_status_t status) {
    send_ok = (status == ESP_NOW_SEND_SUCCESS);
    send_done = true;
}

bool read_climate(float &temp_c, float &humidity) {
    Adafruit_SHT4x sht4;
    if (!sht4.begin()) {
        DEBUG_PRINTF("SHT4x not found\n");
        return false;
    }

    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    sensors_event_t humidity_event, temp_event;
    if (!sht4.getEvent(&humidity_event, &temp_event)) {
        DEBUG_PRINTF("SHT4x read failed\n");
        return false;
    }

    temp_c = temp_event.temperature;
    humidity = humidity_event.relative_humidity;

    // Near condensing the sensor can stay wet and read 100% indefinitely.  Heat it now so the
    // next reading is taken from a dry sensor; this reading has already been taken.
    if (humidity >= HEATER_HUMIDITY_THRESHOLD) {
        sht4.setHeater(SHT4X_MED_HEATER_100MS);
        sht4.getEvent(&humidity_event, &temp_event);
    }

    return true;
}

// Call with the radio on.  A lithium cell's resting voltage stays flat until it's nearly empty, but
// its voltage under load sags more as it wears, so the ~100mA radio draw makes this more telling.
uint16_t read_battery_mv() {
#ifdef BATTERY_ADC_PIN
#ifdef BATTERY_ENABLE_PIN
    pinMode(BATTERY_ENABLE_PIN, OUTPUT);
    digitalWrite(BATTERY_ENABLE_PIN, HIGH);
    delay(BATTERY_SETTLE_MS);
#endif

    uint32_t total_mv = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        total_mv += analogReadMilliVolts(BATTERY_ADC_PIN);
    }

#ifdef BATTERY_ENABLE_PIN
    // An external pull-down on the MOSFET gate keeps the divider off once this pin floats in deep sleep
    digitalWrite(BATTERY_ENABLE_PIN, LOW);
#endif

    return (uint16_t)((total_mv / BATTERY_SAMPLES) * BATTERY_DIVIDER);
#else
    return 0;
#endif
}

void init_charge_counter() {
    // RTC memory only loses power when the battery comes out, which is also the only way to get a
    // power-on reset, so that's taken to mean a fresh battery.  Brownouts, watchdog resets and deep
    // sleep wakes all keep the running total.
    if (esp_reset_reason() == ESP_RST_POWERON || charge_magic != CHARGE_MAGIC) {
        charge_magic = CHARGE_MAGIC;
        charge_used_mah = 0;
    }
}

// Add the charge used by this wake and the sleep that's about to follow
void count_charge() {
    double awake_ms = millis() + BOOT_OVERHEAD_MS;
    charge_used_mah += awake_ms * AWAKE_CURRENT_MA / 3600000.0;
    charge_used_mah += (SLEEP_CURRENT_UA / 1000.0) * SLEEP_S / 3600.0;
}

bool init_espnow() {
    WiFi.mode(WIFI_STA);

    // Setting the channel only sticks while the radio isn't associated; promiscuous mode is the
    // usual trick for getting the driver to accept it.
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        DEBUG_PRINTF("esp_now_init failed\n");
        return false;
    }
    esp_now_register_send_cb(on_sent);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, gateway_mac, sizeof(gateway_mac));
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        DEBUG_PRINTF("esp_now_add_peer failed\n");
        return false;
    }

    return true;
}

// Unicast sends are ACKed by the gateway radio, so a successful send means it was received
bool send_packet(SensorPacket &packet) {
    for (uint8_t attempt = 0; attempt < SEND_ATTEMPTS; attempt++) {
        packet.attempt = attempt;
        send_done = false;
        send_ok = false;

        if (esp_now_send(gateway_mac, (const uint8_t *)&packet, sizeof(packet)) == ESP_OK) {
            unsigned long start_ms = millis();
            while (!send_done && millis() - start_ms < SEND_ACK_TIMEOUT_MS) {
                delay(1);
            }

            if (send_ok) {
                DEBUG_PRINTF("Sent seq %u on attempt %u\n", packet.seq, attempt);
                return true;
            }
        }

        delay(SEND_RETRY_DELAY_MS);
    }

    DEBUG_PRINTF("Send failed after %d attempts\n", SEND_ATTEMPTS);
    return false;
}

void sleep_until_next_reading() {
    count_charge();

    esp_now_deinit();
    WiFi.mode(WIFI_OFF);

    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_S * 1000000ULL);
    esp_deep_sleep_start();
}

void setup() {
#ifdef SENSOR_DEBUG
    Serial.begin(115200);
    // Give USB CDC a moment to reconnect after waking
    delay(1000);
#endif

    init_charge_counter();

    float temp_c, humidity;
    if (!read_climate(temp_c, humidity)) {
        sleep_until_next_reading();
    }

    SensorPacket packet = {};
    packet.magic = SENSOR_PACKET_MAGIC;
    packet.version = SENSOR_PACKET_VERSION;
    packet.missed = missed;
    packet.seq = seq++;
    packet.temp_centi_c = (int16_t)lroundf(temp_c * 100.0f);
    packet.humidity_centi = (uint16_t)lroundf(humidity * 100.0f);
    packet.battery_used_mah = (uint16_t)lround(charge_used_mah);
    packet.battery_capacity_mah = BATTERY_CAPACITY_MAH;

    // Start the radio before reading the battery so the reading is taken under load
    bool radio_ok = init_espnow();
    packet.battery_mv = read_battery_mv();

    DEBUG_PRINTF("temp=%.2fC humidity=%.2f%% battery=%umV used=%.3fmAh\n",
                 temp_c, humidity, packet.battery_mv, charge_used_mah);

    if (radio_ok && send_packet(packet)) {
        missed = 0;
    } else if (missed < UINT8_MAX) {
        missed++;
    }

    sleep_until_next_reading();
}

void loop() {
    // Never reached; every wake ends in deep sleep from setup()
}
