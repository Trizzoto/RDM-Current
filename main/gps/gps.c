#include "gps.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "GPS";

// Forward declarations
static void calculate_ubx_checksum(uint8_t* data, int len, uint8_t* ck_a, uint8_t* ck_b);
static bool test_gps_baud_rate(uint32_t baud_rate);

// Static GPS data
static gps_data_t gps_data = {
    .fix_valid = false,
    .latitude = 0.0,
    .longitude = 0.0,
    .speed_knots = 0.0,
    .speed_kmh = 0.0,
    .course = 0.0,
    .altitude = 0.0,
    .satellites = 0,
    .time_str = {0},
    .date_str = {0},
    .last_update = 0,
    .detected_baud_rate = GPS_BAUD_RATE  // Initialize with default baud rate
};
static SemaphoreHandle_t gps_mutex = NULL;
static TaskHandle_t gps_task_handle = NULL;
static bool gps_initialized = false;
static bool gps_communication_active = false;  // Track if GPS is actually communicating

// Parse NMEA sentence
static void parse_nmea_sentence(const char* sentence) {
    if (strncmp(sentence, "$GPRMC", 6) == 0 || strncmp(sentence, "$GNRMC", 6) == 0) {
        // Parse RMC: time, validity, lat, lon, speed, course, date
        char* tokens[12];
        char* sentence_copy = strdup(sentence);
        if (!sentence_copy) return;
        
        int token_count = 0;
        char* token = strtok(sentence_copy, ",");
        while (token && token_count < 12) {
            tokens[token_count++] = token;
            token = strtok(NULL, ",");
        }
        
        if (token_count >= 9) {
            if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                // Validity
                gps_data.fix_valid = (tokens[2][0] == 'A');
                
                if (gps_data.fix_valid) {
                    // Parse latitude
                    if (strlen(tokens[3]) > 0 && strlen(tokens[4]) > 0) {
                        double lat_deg = atof(tokens[3]) / 100.0;
                        int lat_deg_int = (int)lat_deg;
                        double lat_min = (lat_deg - lat_deg_int) * 100.0;
                        gps_data.latitude = lat_deg_int + lat_min / 60.0;
                        if (tokens[4][0] == 'S') gps_data.latitude = -gps_data.latitude;
                    }
                    
                    // Parse longitude
                    if (strlen(tokens[5]) > 0 && strlen(tokens[6]) > 0) {
                        double lon_deg = atof(tokens[5]) / 100.0;
                        int lon_deg_int = (int)lon_deg;
                        double lon_min = (lon_deg - lon_deg_int) * 100.0;
                        gps_data.longitude = lon_deg_int + lon_min / 60.0;
                        if (tokens[6][0] == 'W') gps_data.longitude = -gps_data.longitude;
                    }
                    
                    // Parse speed (knots to km/h)
                    if (strlen(tokens[7]) > 0) {
                        gps_data.speed_knots = atof(tokens[7]);
                        gps_data.speed_kmh = gps_data.speed_knots * 1.852f;
                    }
                    
                    // Parse course
                    if (strlen(tokens[8]) > 0) {
                        gps_data.course = atof(tokens[8]);
                    }
                    
                    // Parse time and date
                    if (strlen(tokens[1]) >= 6) {
                        snprintf(gps_data.time_str, sizeof(gps_data.time_str), 
                                "%.2s:%.2s:%.2s", tokens[1], tokens[1]+2, tokens[1]+4);
                    }
                    if (strlen(tokens[9]) >= 6) {
                        snprintf(gps_data.date_str, sizeof(gps_data.date_str), 
                                "%.2s/%.2s/%.2s", tokens[9], tokens[9]+2, tokens[9]+4);
                    }
                    
                    gps_data.last_update = esp_timer_get_time() / 1000;
                }
                xSemaphoreGive(gps_mutex);
            }
        }
        free(sentence_copy);
    }
    else if (strncmp(sentence, "$GPGGA", 6) == 0 || strncmp(sentence, "$GNGGA", 6) == 0) {
        // Parse GGA: time, lat, lon, fix quality, satellites, altitude
        char* tokens[15];
        char* sentence_copy = strdup(sentence);
        if (!sentence_copy) return;
        
        int token_count = 0;
        char* token = strtok(sentence_copy, ",");
        while (token && token_count < 15) {
            tokens[token_count++] = token;
            token = strtok(NULL, ",");
        }
        
        if (token_count >= 10) {
            if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                // Satellites
                if (strlen(tokens[7]) > 0) {
                    gps_data.satellites = atoi(tokens[7]);
                }
                
                // Altitude
                if (strlen(tokens[9]) > 0) {
                    gps_data.altitude = atof(tokens[9]);
                }
                
                xSemaphoreGive(gps_mutex);
            }
        }
        free(sentence_copy);
    }
}

// Test GPS communication at specific baud rate
static bool test_gps_baud_rate(uint32_t baud_rate) {
    ESP_LOGI(TAG, "Testing GPS communication at %lu baud...", baud_rate);
    
    // Configure UART for test baud rate
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(GPS_UART_NUM, &uart_config);
    
    // Flush any old data
    uart_flush(GPS_UART_NUM);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Listen for NMEA data for 3 seconds
    uint8_t buffer[256];
    bool nmea_found = false;
    int attempts = 0;
    
    while (attempts < 30 && !nmea_found) { // 3 seconds total
        int len = uart_read_bytes(GPS_UART_NUM, buffer, sizeof(buffer), pdMS_TO_TICKS(100));
        if (len > 0) {
            // Look for NMEA sentence start
            for (int i = 0; i < len - 5; i++) {
                if (buffer[i] == '$' && 
                    (strncmp((char*)&buffer[i], "$GPRMC", 6) == 0 ||
                     strncmp((char*)&buffer[i], "$GNRMC", 6) == 0 ||
                     strncmp((char*)&buffer[i], "$GPGGA", 6) == 0 ||
                     strncmp((char*)&buffer[i], "$GNGGA", 6) == 0)) {
                    ESP_LOGI(TAG, "GPS NMEA data detected at %lu baud", baud_rate);
                    nmea_found = true;
                    break;
                }
            }
        }
        attempts++;
    }
    
    return nmea_found;
}

// Configure GPS for higher baud rate for faster data transfer
esp_err_t gps_configure_high_baud_rate(void) {
    ESP_LOGI(TAG, "Configuring GPS for 38400 baud rate...");
    
    // UBX-CFG-PRT command to set 38400 baud rate
    uint8_t ubx_cfg_prt[] = {
        0xB5, 0x62,  // UBX sync chars
        0x06, 0x00,  // Class CFG, ID PRT
        0x14, 0x00,  // Length: 20 bytes
        0x01,        // Port ID: 1 (UART)
        0x00,        // Reserved
        0x00, 0x00,  // TX ready
        0xD0, 0x08, 0x00, 0x00,  // Mode: 8N1
        0x00, 0x96, 0x00, 0x00,  // Baud rate: 38400 (little endian)
        0x07, 0x00,  // Input protocols: NMEA + UBX
        0x03, 0x00,  // Output protocols: NMEA + UBX  
        0x00, 0x00,  // Flags
        0x00, 0x00,  // Reserved
        0x00, 0x00   // Checksum (will be calculated)
    };
    
    // Calculate and set checksum
    uint8_t ck_a, ck_b;
    calculate_ubx_checksum(ubx_cfg_prt, sizeof(ubx_cfg_prt), &ck_a, &ck_b);
    ubx_cfg_prt[sizeof(ubx_cfg_prt) - 2] = ck_a;
    ubx_cfg_prt[sizeof(ubx_cfg_prt) - 1] = ck_b;
    
    // Send command at current baud rate
    uart_write_bytes(GPS_UART_NUM, ubx_cfg_prt, sizeof(ubx_cfg_prt));
    uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
    
    // Wait for GPS to change baud rate
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Reconfigure UART to 38400 baud
    uart_config_t uart_config = {
        .baud_rate = 38400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &uart_config));
    
    ESP_LOGI(TAG, "GPS configured for 38400 baud rate");
    return ESP_OK;
}

// GPS task - OPTIMIZED FOR LOW LATENCY
static void gps_task(void* arg) {
    uint8_t* data = malloc(GPS_BUFFER_SIZE);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate GPS buffer");
        vTaskDelete(NULL);
        return;
    }
    
    char nmea_buffer[512] = {0};
    int nmea_index = 0;
    uint32_t sentence_count = 0;
    uint64_t last_data_time = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "GPS task started - listening at %d baud", GPS_BAUD_RATE);
    
    while (1) {
        // REDUCED TIMEOUT for faster response - was 1000ms, now 10ms
        int len = uart_read_bytes(GPS_UART_NUM, data, GPS_BUFFER_SIZE, pdMS_TO_TICKS(10));
        if (len > 0) {
            last_data_time = esp_timer_get_time() / 1000;
            
            if (!gps_communication_active) {
                ESP_LOGI(TAG, "GPS communication reestablished");
                gps_communication_active = true;
            }
            
            // Process data immediately for minimum latency
            for (int i = 0; i < len; i++) {
                char c = data[i];
                
                if (c == '$') {
                    nmea_index = 0;
                    nmea_buffer[nmea_index++] = c;
                } else if (c == '\n' || c == '\r') {
                    if (nmea_index > 0) {
                        nmea_buffer[nmea_index] = '\0';
                        if (nmea_index > 10) { // Valid NMEA sentence
                            sentence_count++;
                            // IMMEDIATE PARSING - no delay
                            parse_nmea_sentence(nmea_buffer);
                        }
                        nmea_index = 0;
                    }
                } else if (nmea_index < sizeof(nmea_buffer) - 1) {
                    nmea_buffer[nmea_index++] = c;
                }
            }
        } else {
            // Check for communication timeout (3 seconds for faster detection)
            uint64_t current_time = esp_timer_get_time() / 1000;
            if (gps_communication_active && (current_time - last_data_time) > 3000) {
                ESP_LOGW(TAG, "GPS communication timeout - no data for 3 seconds");
                gps_communication_active = false;
            }
        }
        
        // YIELD to prevent watchdog but minimize delay
        vTaskDelay(pdMS_TO_TICKS(1)); // Was no delay, now 1ms for system stability
    }
    
    free(data);
    vTaskDelete(NULL);
}

// Initialize GPS - FAST 38400 BAUD ONLY
esp_err_t gps_init(void) {
    if (gps_initialized) {
        ESP_LOGW(TAG, "GPS already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing GPS on UART%d (TX:%d, RX:%d) at 38400 baud", 
             GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN);
    
    // Create mutex
    gps_mutex = xSemaphoreCreateMutex();
    if (!gps_mutex) {
        ESP_LOGE(TAG, "Failed to create GPS mutex");
        return ESP_FAIL;
    }
    
    // UART configuration - directly use 38400 baud (known GPS baud rate)
    uart_config_t uart_config = {
        .baud_rate = GPS_BAUD_RATE,  // Fixed 38400 baud
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_NUM, GPS_BUFFER_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Proper GPS boot delay - GPS modules need time to initialize
    ESP_LOGI(TAG, "GPS boot delay for proper initialization...");
    vTaskDelay(pdMS_TO_TICKS(2000));  // Proper delay for GPS boot
    
    // Detect GPS baud rate
    ESP_LOGI(TAG, "Detecting GPS baud rate...");
    const uint32_t test_bauds[] = {38400, 9600, 57600, 115200, 4800};
    const int num_bauds = sizeof(test_bauds) / sizeof(test_bauds[0]);
    bool baud_detected = false;
    uint32_t detected_baud = GPS_BAUD_RATE;
    
    for (int i = 0; i < num_bauds && !baud_detected; i++) {
        if (test_gps_baud_rate(test_bauds[i])) {
            detected_baud = test_bauds[i];
            baud_detected = true;
            ESP_LOGI(TAG, "GPS detected at %lu baud", detected_baud);
            break;
        }
    }
    
    if (!baud_detected) {
        ESP_LOGW(TAG, "GPS not detected at any baud rate, defaulting to %d", GPS_BAUD_RATE);
        detected_baud = GPS_BAUD_RATE;
    }
    
    // Set final baud rate
    uart_config_t final_config = {
        .baud_rate = detected_baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &final_config));
    
    // Start with communication inactive - will be set true when data received
    gps_communication_active = baud_detected;
    
    if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        gps_data.detected_baud_rate = detected_baud;
        xSemaphoreGive(gps_mutex);
    }
    
    // Create GPS task with moderate priority (don't interfere with system tasks)
    ESP_LOGI(TAG, "Starting GPS task...");
    if (xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, &gps_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create GPS task");
        return ESP_FAIL;
    }
    
    // Wait for GPS task to detect communication before configuring
    ESP_LOGI(TAG, "Waiting for GPS communication...");
    int wait_count = 0;
    while (!gps_communication_active && wait_count < 30) {  // Wait up to 30 seconds
        vTaskDelay(pdMS_TO_TICKS(1000));
        wait_count++;
        ESP_LOGI(TAG, "GPS communication check... %d/30", wait_count);
    }
    
    if (gps_communication_active) {
        ESP_LOGI(TAG, "GPS communication established! Configuring for optimal performance...");
        vTaskDelay(pdMS_TO_TICKS(500));  // Brief delay before configuration
        
        // Configure 10Hz update rate
        gps_configure_10hz();
        
        // Configure for ultra low latency (maximum responsiveness)
        gps_configure_ultra_low_latency_mode();
    } else {
        ESP_LOGW(TAG, "GPS communication not established - will continue trying in background");
    }
    
    gps_initialized = true;
    ESP_LOGI(TAG, "GPS initialization completed at %lu baud", detected_baud);
    return ESP_OK;
}

// Deinitialize GPS
esp_err_t gps_deinit(void) {
    if (!gps_initialized) {
        return ESP_OK;
    }
    
    // Delete task
    if (gps_task_handle) {
        vTaskDelete(gps_task_handle);
        gps_task_handle = NULL;
    }
    
    // Delete mutex
    if (gps_mutex) {
        vSemaphoreDelete(gps_mutex);
        gps_mutex = NULL;
    }
    
    // Uninstall UART driver
    uart_driver_delete(GPS_UART_NUM);
    
    gps_initialized = false;
    ESP_LOGI(TAG, "GPS deinitialized");
    return ESP_OK;
}

// Get GPS data
bool gps_get_data(gps_data_t* data) {
    if (!gps_initialized || !data || !gps_mutex) {
        return false;
    }
    
    // If no communication is active, return false (GPS disconnected)
    if (!gps_communication_active) {
        return false;
    }
    
    if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        memcpy(data, &gps_data, sizeof(gps_data_t));
        xSemaphoreGive(gps_mutex);
        return true;
    }
    return false;
}

// Get speed in km/h
float gps_get_speed_kmh(void) {
    if (!gps_initialized || !gps_mutex) {
        return 0.0f;
    }
    
    float speed = 0.0f;
    if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        speed = gps_data.speed_kmh;
        xSemaphoreGive(gps_mutex);
    }
    return speed;
}

// Check if GPS fix is valid
bool gps_is_fix_valid(void) {
    if (!gps_initialized || !gps_mutex) {
        return false;
    }
    
    bool valid = false;
    if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        valid = gps_data.fix_valid;
        xSemaphoreGive(gps_mutex);
    }
    return valid;
}

// Get satellite count
uint8_t gps_get_satellite_count(void) {
    if (!gps_initialized || !gps_mutex) {
        return 0;
    }
    
    uint8_t count = 0;
    if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        count = gps_data.satellites;
        xSemaphoreGive(gps_mutex);
    }
    return count;
}

// Calculate UBX checksum
static void calculate_ubx_checksum(uint8_t* data, int len, uint8_t* ck_a, uint8_t* ck_b) {
    *ck_a = 0;
    *ck_b = 0;
    for (int i = 2; i < len - 2; i++) {
        *ck_a += data[i];
        *ck_b += *ck_a;
    }
}

// Factory reset GPS module to clear EEPROM settings
esp_err_t gps_factory_reset(void) {
    ESP_LOGI(TAG, "Performing GPS factory reset...");
    
    // UBX-CFG-CFG command to clear configuration and reset to defaults
    // This clears all saved settings in EEPROM/Flash and reverts to factory defaults
    uint8_t ubx_factory_reset[] = {
        0xB5, 0x62,  // UBX sync chars
        0x06, 0x09,  // Class CFG, ID CFG
        0x0D, 0x00,  // Length: 13 bytes
        0xFF, 0xFF, 0x00, 0x00,  // Clear mask: clear all
        0x00, 0x00, 0x00, 0x00,  // Save mask: don't save anything  
        0x00, 0x00, 0x00, 0x00,  // Load mask: don't load anything
        0x17,        // Device: BBR, Flash, EEPROM, SPI Flash
        0x00, 0x00   // Checksum (will be calculated)
    };
    
    // Calculate and set checksum
    uint8_t ck_a, ck_b;
    calculate_ubx_checksum(ubx_factory_reset, sizeof(ubx_factory_reset), &ck_a, &ck_b);
    ubx_factory_reset[sizeof(ubx_factory_reset) - 2] = ck_a;
    ubx_factory_reset[sizeof(ubx_factory_reset) - 1] = ck_b;
    
    // Try sending reset command at multiple baud rates
    const uint32_t reset_bauds[] = {9600, 38400, 57600, 115200, 4800};
    const int num_reset_bauds = sizeof(reset_bauds) / sizeof(reset_bauds[0]);
    
    for (int i = 0; i < num_reset_bauds; i++) {
        ESP_LOGI(TAG, "Sending factory reset at %lu baud", reset_bauds[i]);
        
        // Configure UART for this baud rate
        uart_config_t uart_config = {
            .baud_rate = reset_bauds[i],
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        uart_param_config(GPS_UART_NUM, &uart_config);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Send UBX factory reset command
        uart_write_bytes(GPS_UART_NUM, ubx_factory_reset, sizeof(ubx_factory_reset));
        uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Also try NMEA factory reset commands (backup method)
        const char* nmea_reset1 = "$PUBX,41,1,0007,0003,9600,0*10\r\n";  // Factory reset + 9600 baud
        const char* nmea_reset2 = "$PUBX,41,1,0007,0001,9600,0*12\r\n";  // Cold start + 9600 baud
        
        uart_write_bytes(GPS_UART_NUM, nmea_reset1, strlen(nmea_reset1));
        uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
        vTaskDelay(pdMS_TO_TICKS(500));
        
        uart_write_bytes(GPS_UART_NUM, nmea_reset2, strlen(nmea_reset2));
        uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
        
        // Flash GPIO 16 to show reset attempt
        gpio_set_level(16, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(16, 0);
        
        // Wait for reset to complete
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Factory reset commands sent - GPS should restart with defaults");
    
    // Wait for GPS to restart (modules typically take 3-5 seconds)
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Flash GPIO 16 rapidly to indicate reset completion
    for (int i = 0; i < 6; i++) {
        gpio_set_level(16, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(16, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    return ESP_OK;
}

// Check if GPS communication is active
bool gps_is_communication_active(void) {
    return gps_communication_active;
}

// Configure GPS for 10Hz update rate
esp_err_t gps_configure_10hz(void) {
    ESP_LOGI(TAG, "Configuring GPS for 10Hz update rate...");
    
    // UBX-CFG-RATE command to set 100ms measurement rate (10Hz)
    // This sets the measurement period to 100ms = 10Hz
    uint8_t ubx_cfg_rate[] = {
        0xB5, 0x62,  // UBX sync chars
        0x06, 0x08,  // Class CFG, ID RATE
        0x06, 0x00,  // Length: 6 bytes
        0x64, 0x00,  // Measurement rate: 100ms (little endian)
        0x01, 0x00,  // Navigation rate: 1 (every measurement)
        0x01, 0x00,  // Time reference: 1 (GPS time)
        0x00, 0x00   // Checksum (will be calculated)
    };
    
    // Calculate and set checksum
    uint8_t ck_a, ck_b;
    calculate_ubx_checksum(ubx_cfg_rate, sizeof(ubx_cfg_rate), &ck_a, &ck_b);
    ubx_cfg_rate[sizeof(ubx_cfg_rate) - 2] = ck_a;
    ubx_cfg_rate[sizeof(ubx_cfg_rate) - 1] = ck_b;
    
    // Send the configuration command
    uart_write_bytes(GPS_UART_NUM, ubx_cfg_rate, sizeof(ubx_cfg_rate));
    uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
    
    // Wait for GPS to process the command
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Also try NMEA command for non-u-blox modules
    const char* nmea_10hz = "$PMTK220,100*2F\r\n";  // MediaTek 10Hz command
    uart_write_bytes(GPS_UART_NUM, nmea_10hz, strlen(nmea_10hz));
    uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
    
    // Save configuration to flash/EEPROM so it persists after power cycle
    uint8_t ubx_cfg_cfg[] = {
        0xB5, 0x62,  // UBX sync chars
        0x06, 0x09,  // Class CFG, ID CFG
        0x0D, 0x00,  // Length: 13 bytes
        0x00, 0x00, 0x00, 0x00,  // Clear mask: don't clear anything
        0xFF, 0xFF, 0x00, 0x00,  // Save mask: save all current config
        0x00, 0x00, 0x00, 0x00,  // Load mask: don't load anything
        0x17,        // Device: BBR, Flash, EEPROM, SPI Flash
        0x00, 0x00   // Checksum (will be calculated)
    };
    
    // Calculate and set checksum for save command
    calculate_ubx_checksum(ubx_cfg_cfg, sizeof(ubx_cfg_cfg), &ck_a, &ck_b);
    ubx_cfg_cfg[sizeof(ubx_cfg_cfg) - 2] = ck_a;
    ubx_cfg_cfg[sizeof(ubx_cfg_cfg) - 1] = ck_b;
    
    // Send save configuration command
    uart_write_bytes(GPS_UART_NUM, ubx_cfg_cfg, sizeof(ubx_cfg_cfg));
    uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "GPS configured for 10Hz update rate and saved to flash");
    
    return ESP_OK;
}

// Configure GPS for ultra low latency mode for instant raw readings (no filtering)
esp_err_t gps_configure_ultra_low_latency_mode(void) {
    ESP_LOGI(TAG, "Configuring GPS for ULTRA LOW LATENCY - removing all filtering...");
    
    // UBX-CFG-NAV5 command with NO filtering for instant raw readings
    uint8_t ubx_cfg_nav5[] = {
        0xB5, 0x62,  // UBX sync chars
        0x06, 0x24,  // Class CFG, ID NAV5
        0x24, 0x00,  // Length: 36 bytes
        0xFF, 0xFF,  // Parameter mask: apply all settings
        0x06,        // Dynamic model: 6 = Airborne <1g (FASTEST RESPONSE)
        0x03,        // Fix mode: 3 = Auto 2D/3D
        0x00, 0x00, 0x00, 0x00,  // Fixed altitude (not used in auto mode)
        0x10, 0x27, 0x00, 0x00,  // Fixed altitude variance (not used)
        0x00,        // Minimum elevation: 0 degrees (accept all satellites)
        0x00,        // Reserved
        0x00, 0x00,  // Position DOP mask: 0 (DISABLED - no filtering)
        0x00, 0x00,  // Time DOP mask: 0 (DISABLED - no filtering)
        0x00, 0x00,  // Position accuracy mask: 0 (DISABLED - accept all readings)
        0x00, 0x00,  // Time accuracy mask: 0 (DISABLED - no time averaging)
        0x00,        // Static hold threshold: 0 (DISABLED for max responsiveness)
        0x00,        // DGPS timeout: 0 (DISABLED)
        0x00,        // CNO threshold: 0 (DISABLED - accept weak signals)
        0x00,        // Reserved
        0x00, 0x00,  // Static hold distance: 0 (DISABLED for max responsiveness)
        0x00,        // UTC standard: 0 (automatic)
        0x00, 0x00, 0x00, 0x00, 0x00,  // Reserved
        0x00, 0x00   // Checksum (will be calculated)
    };
    
    // Calculate and set checksum
    uint8_t ck_a, ck_b;
    calculate_ubx_checksum(ubx_cfg_nav5, sizeof(ubx_cfg_nav5), &ck_a, &ck_b);
    ubx_cfg_nav5[sizeof(ubx_cfg_nav5) - 2] = ck_a;
    ubx_cfg_nav5[sizeof(ubx_cfg_nav5) - 1] = ck_b;
    
    // Send the ultra low latency configuration command
    uart_write_bytes(GPS_UART_NUM, ubx_cfg_nav5, sizeof(ubx_cfg_nav5));
    uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
    
    // Wait for GPS to process the command
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Configure message output for fastest possible updates
    // Enable RMC and VTG messages only (minimal data for speed)
    uint8_t ubx_cfg_msg_rmc[] = {
        0xB5, 0x62,  // UBX sync chars
        0x06, 0x01,  // Class CFG, ID MSG
        0x08, 0x00,  // Length: 8 bytes
        0xF0, 0x04,  // Message Class/ID: NMEA RMC
        0x01,        // Rate on DDC: 1 (every fix)
        0x01,        // Rate on UART1: 1 (every fix)
        0x00,        // Rate on UART2: 0 (disabled)
        0x01,        // Rate on USB: 1 (every fix)
        0x00,        // Rate on SPI: 0 (disabled)
        0x00,        // Reserved
        0x00, 0x00   // Checksum (will be calculated)
    };
    
    calculate_ubx_checksum(ubx_cfg_msg_rmc, sizeof(ubx_cfg_msg_rmc), &ck_a, &ck_b);
    ubx_cfg_msg_rmc[sizeof(ubx_cfg_msg_rmc) - 2] = ck_a;
    ubx_cfg_msg_rmc[sizeof(ubx_cfg_msg_rmc) - 1] = ck_b;
    
    uart_write_bytes(GPS_UART_NUM, ubx_cfg_msg_rmc, sizeof(ubx_cfg_msg_rmc));
    uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
    
    // Disable other NMEA messages to reduce processing overhead
    // This reduces the amount of data and speeds up parsing
    const uint8_t disable_messages[][2] = {
        {0xF0, 0x00}, // GGA
        {0xF0, 0x01}, // GLL
        {0xF0, 0x02}, // GSA
        {0xF0, 0x03}, // GSV
        {0xF0, 0x05}, // VTG (we'll enable this separately)
    };
    
    for (int i = 0; i < 5; i++) {
        uint8_t ubx_disable[] = {
            0xB5, 0x62,  // UBX sync chars
            0x06, 0x01,  // Class CFG, ID MSG
            0x08, 0x00,  // Length: 8 bytes
            disable_messages[i][0], disable_messages[i][1],  // Message Class/ID
            0x00,        // Rate on DDC: 0 (disabled)
            0x00,        // Rate on UART1: 0 (disabled)
            0x00,        // Rate on UART2: 0 (disabled)
            0x00,        // Rate on USB: 0 (disabled)
            0x00,        // Rate on SPI: 0 (disabled)
            0x00,        // Reserved
            0x00, 0x00   // Checksum (will be calculated)
        };
        
        calculate_ubx_checksum(ubx_disable, sizeof(ubx_disable), &ck_a, &ck_b);
        ubx_disable[sizeof(ubx_disable) - 2] = ck_a;
        ubx_disable[sizeof(ubx_disable) - 1] = ck_b;
        
        uart_write_bytes(GPS_UART_NUM, ubx_disable, sizeof(ubx_disable));
        uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(500));
    }
    
    // Save configuration to flash/EEPROM
    uint8_t ubx_cfg_cfg[] = {
        0xB5, 0x62,  // UBX sync chars
        0x06, 0x09,  // Class CFG, ID CFG
        0x0D, 0x00,  // Length: 13 bytes
        0x00, 0x00, 0x00, 0x00,  // Clear mask: don't clear anything
        0xFF, 0xFF, 0x00, 0x00,  // Save mask: save all current config
        0x00, 0x00, 0x00, 0x00,  // Load mask: don't load anything
        0x17,        // Device: BBR, Flash, EEPROM, SPI Flash
        0x00, 0x00   // Checksum (will be calculated)
    };
    
    // Calculate and set checksum for save command
    calculate_ubx_checksum(ubx_cfg_cfg, sizeof(ubx_cfg_cfg), &ck_a, &ck_b);
    ubx_cfg_cfg[sizeof(ubx_cfg_cfg) - 2] = ck_a;
    ubx_cfg_cfg[sizeof(ubx_cfg_cfg) - 1] = ck_b;
    
    // Send save configuration command
    uart_write_bytes(GPS_UART_NUM, ubx_cfg_cfg, sizeof(ubx_cfg_cfg));
    uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "GPS configured for ULTRA LOW LATENCY (no filtering, raw readings) and saved to flash");
    
    return ESP_OK;
}

// Add new automotive mode configuration (instead of ultra low latency)
esp_err_t gps_configure_automotive_mode(void) {
    ESP_LOGI(TAG, "Configuring GPS for automotive mode...");
    
    // UBX-CFG-NAV5 command for automotive dynamic model
    uint8_t ubx_cfg_nav5[] = {
        0xB5, 0x62,  // UBX sync chars
        0x06, 0x24,  // Class CFG, ID NAV5
        0x24, 0x00,  // Length: 36 bytes
        0xFF, 0xFF,  // Parameter mask: apply all settings
        0x04,        // Dynamic model: 4 = Automotive (good balance)
        0x03,        // Fix mode: 3 = Auto 2D/3D
        0x00, 0x00, 0x00, 0x00,  // Fixed altitude (not used)
        0x10, 0x27, 0x00, 0x00,  // Fixed altitude variance
        0x05,        // Minimum elevation: 5 degrees
        0x00,        // Reserved
        0xFA, 0x00,  // Position DOP mask: 25.0
        0xFA, 0x00,  // Time DOP mask: 25.0
        0x64, 0x00,  // Position accuracy mask: 100m
        0x2C, 0x01,  // Time accuracy mask: 300s
        0x00,        // Static hold threshold: 0 (disabled)
        0x3C,        // DGPS timeout: 60s
        0x00,        // CNO threshold: 0 (default)
        0x00,        // Reserved
        0x00, 0x00,  // Static hold distance: 0
        0x00,        // UTC standard: 0 (automatic)
        0x00, 0x00, 0x00, 0x00, 0x00,  // Reserved
        0x00, 0x00   // Checksum (will be calculated)
    };
    
    // Calculate and set checksum
    uint8_t ck_a, ck_b;
    calculate_ubx_checksum(ubx_cfg_nav5, sizeof(ubx_cfg_nav5), &ck_a, &ck_b);
    ubx_cfg_nav5[sizeof(ubx_cfg_nav5) - 2] = ck_a;
    ubx_cfg_nav5[sizeof(ubx_cfg_nav5) - 1] = ck_b;
    
    // Send configuration
    uart_write_bytes(GPS_UART_NUM, ubx_cfg_nav5, sizeof(ubx_cfg_nav5));
    uart_wait_tx_done(GPS_UART_NUM, pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "GPS configured for automotive mode");
    return ESP_OK;
} 