#ifndef REMOTE_SENSOR_PROTOCOL_H
#define REMOTE_SENSOR_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

//----------------------------------------------------
// ESP-NOW link: battery sensor -> gateway radio

// 2.4GHz channel the gateway radio listens on.  Pick one the UniFi APs are not using so the
// sensors aren't competing with WiFi traffic.  Sensors and gateway radio must agree.
#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL 11
#endif

// The gateway radio replaces its station MAC with this locally administered address, so every
// sensor can send to it without being told which board the gateway is.
#define GATEWAY_RADIO_MAC {0x02, 0x47, 0x48, 0x00, 0x00, 0x01}

#define SENSOR_PACKET_MAGIC 0x4748
#define SENSOR_PACKET_VERSION 2

struct __attribute__((packed)) SensorPacket {
    uint16_t magic;
    uint8_t version;
    uint8_t attempt;        // 0 on the first send attempt this wake, 1 on the first retry, ...
    uint8_t missed;         // wakes since the last delivered packet where every attempt failed
    uint32_t seq;
    int16_t temp_centi_c;   // 2341 == 23.41C
    uint16_t humidity_centi; // 8120 == 81.20%
    uint16_t battery_mv;    // measured with the radio on; 0 when the sensor has no battery divider
    uint16_t battery_used_mah;     // estimated charge used since the battery was put in
    uint16_t battery_capacity_mah; // usable capacity of the battery the sensor was built for
};

//----------------------------------------------------
// UART link: gateway radio -> gateway uplink
//
// One reading per line, NMEA style, with an XOR checksum over the characters between '$' and '*':
//
//   $GH,<mac hex>,<seq>,<temp_centi_c>,<humidity_centi>,<battery_mv>,<rssi>,<attempt>,<missed>,
//       <battery_used_mah>,<battery_capacity_mah>*<XX>\n   (all on one line)

#define UART_LINK_BAUD 115200
#define UART_LINK_MAX_LINE 128

inline uint8_t uart_link_checksum(const char *start, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= (uint8_t)start[i];
    }
    return sum;
}

#endif
