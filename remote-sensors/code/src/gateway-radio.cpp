// Gateway radio: listens for sensor packets over ESP-NOW on a fixed channel and writes each one as
// a checksummed line to the uplink board over UART.  It never joins WiFi, so the channel never moves.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "remote_sensor_protocol.h"

//----------------------------------------------------
// Defines

#ifndef UPLINK_TX_PIN
#define UPLINK_TX_PIN D6
#endif
#ifndef UPLINK_RX_PIN
#define UPLINK_RX_PIN D7
#endif

#define RECEIVED_QUEUE_LENGTH 16

// Log a summary to USB serial this often
#define HEARTBEAT_PERIOD_MS (10 * 60 * 1000)

struct ReceivedPacket {
    uint8_t mac[6];
    int8_t rssi;
    SensorPacket packet;
};

//----------------------------------------------------
// Globals

QueueHandle_t received_queue;

volatile uint32_t received_count = 0;
volatile uint32_t dropped_count = 0;
volatile uint32_t rejected_count = 0;

unsigned long last_heartbeat_ms = 0;

#if ESP_IDF_VERSION_MAJOR < 5
// IDF 4.x doesn't hand RSSI to the ESP-NOW callback, so sniff it from the vendor action frame that
// carries the packet.  The promiscuous callback runs just before the ESP-NOW receive callback.
volatile int8_t sniffed_rssi = 0;
uint8_t sniffed_mac[6];

void on_promiscuous(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) {
        return;
    }

    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    const uint8_t *frame = pkt->payload;

    // 0xD0 is an action frame and category 127 is vendor specific, which is what ESP-NOW uses
    if (frame[0] != 0xD0 || frame[24] != 127) {
        return;
    }

    memcpy(sniffed_mac, frame + 10, sizeof(sniffed_mac));
    sniffed_rssi = pkt->rx_ctrl.rssi;
}
#endif

//----------------------------------------------------
// Functions

void queue_packet(const uint8_t *mac, int8_t rssi, const uint8_t *data, int len) {
    if (len != sizeof(SensorPacket)) {
        rejected_count++;
        return;
    }

    ReceivedPacket received;
    memcpy(received.mac, mac, sizeof(received.mac));
    memcpy(&received.packet, data, sizeof(SensorPacket));
    received.rssi = rssi;

    if (received.packet.magic != SENSOR_PACKET_MAGIC || received.packet.version != SENSOR_PACKET_VERSION) {
        rejected_count++;
        return;
    }

    // Runs on the WiFi task, so just hand off and get out
    if (xQueueSend(received_queue, &received, 0) != pdTRUE) {
        dropped_count++;
        return;
    }
    received_count++;
}

#if ESP_IDF_VERSION_MAJOR < 5
void on_receive(const uint8_t *mac, const uint8_t *data, int len) {
    int8_t rssi = memcmp(mac, sniffed_mac, sizeof(sniffed_mac)) == 0 ? sniffed_rssi : 0;
    queue_packet(mac, rssi, data, len);
}
#else
void on_receive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    queue_packet(info->src_addr, info->rx_ctrl->rssi, data, len);
}
#endif

void write_line(const ReceivedPacket &received) {
    const SensorPacket &p = received.packet;
    char line[UART_LINK_MAX_LINE];

    int len = snprintf(line, sizeof(line), "$GH,%02x%02x%02x%02x%02x%02x,%u,%d,%u,%u,%d,%u,%u,%u,%u",
        received.mac[0], received.mac[1], received.mac[2], received.mac[3], received.mac[4], received.mac[5],
        p.seq, p.temp_centi_c, p.humidity_centi, p.battery_mv, received.rssi, p.attempt, p.missed,
        p.battery_used_mah, p.battery_capacity_mah);

    // Checksum covers everything after the '$'
    uint8_t checksum = uart_link_checksum(line + 1, len - 1);
    snprintf(line + len, sizeof(line) - len, "*%02X\n", checksum);

    Serial1.print(line);
    Serial.print(line);
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(UART_LINK_BAUD, SERIAL_8N1, UPLINK_RX_PIN, UPLINK_TX_PIN);

    received_queue = xQueueCreate(RECEIVED_QUEUE_LENGTH, sizeof(ReceivedPacket));

    WiFi.mode(WIFI_STA);

    uint8_t mac[] = GATEWAY_RADIO_MAC;
    if (esp_wifi_set_mac(WIFI_IF_STA, mac) != ESP_OK) {
        Serial.println("Failed to set gateway MAC; sensors will not be able to reach us");
    }

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
#if ESP_IDF_VERSION_MAJOR < 5
    // Stay in promiscuous mode to sniff RSSI; only management frames are needed
    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(on_promiscuous);
#else
    esp_wifi_set_promiscuous(false);
#endif

    if (esp_now_init() != ESP_OK) {
        Serial.println("esp_now_init failed, restarting");
        delay(1000);
        ESP.restart();
    }
    esp_now_register_recv_cb(on_receive);

    Serial.printf("Gateway radio listening on channel %d as %s\n", ESPNOW_CHANNEL, WiFi.macAddress().c_str());
}

void loop() {
    ReceivedPacket received;
    if (xQueueReceive(received_queue, &received, pdMS_TO_TICKS(1000)) == pdTRUE) {
        write_line(received);
    }

    if (millis() - last_heartbeat_ms >= HEARTBEAT_PERIOD_MS) {
        Serial.printf("Gateway radio: received=%u dropped=%u rejected=%u\n", received_count, dropped_count, rejected_count);
        last_heartbeat_ms = millis();
    }
}
