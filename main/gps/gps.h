#ifndef GPS_H
#define GPS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// GPS configuration constants for M8N module - FIXED 38400 BAUD
#define GPS_UART_NUM        UART_NUM_1
#define GPS_TX_PIN          43  // ESP32 TX to GPS RX
#define GPS_RX_PIN          44  // ESP32 RX from GPS TX
#define GPS_BAUD_RATE       38400   // Fixed baud rate - no testing needed
#define GPS_BUFFER_SIZE     256     // Smaller buffer to reduce latency

// GPS data structure
typedef struct {
    bool fix_valid;
    float latitude;
    float longitude;
    float speed_knots;
    float speed_kmh;
    float course;
    float altitude;
    uint8_t satellites;
    char time_str[16];
    char date_str[16];
    uint64_t last_update;
    uint32_t detected_baud_rate;  // Always 38400
} gps_data_t;

// Function declarations
esp_err_t gps_init(void);
esp_err_t gps_deinit(void);
bool gps_get_data(gps_data_t* data);
float gps_get_speed_kmh(void);
bool gps_is_fix_valid(void);
uint8_t gps_get_satellite_count(void);
bool gps_is_communication_active(void);  // Check if GPS is communicating
esp_err_t gps_configure_10hz(void);      // Configure GPS for 10Hz update rate
esp_err_t gps_configure_automotive_mode(void);  // Configure GPS for automotive dynamic mode
esp_err_t gps_configure_airborne_mode(void);    // Configure GPS for airborne <1g mode (max responsiveness)
esp_err_t gps_configure_ultra_low_latency_mode(void);  // Configure GPS for ultra low latency (no filtering, raw readings)
esp_err_t gps_configure_high_baud_rate(void);   // Configure GPS for higher baud rate
esp_err_t gps_factory_reset(void);   // Reset GPS to factory defaults

#ifdef __cplusplus
}
#endif

#endif // GPS_H
