#include "fuel_input.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "FUEL_INPUT";

// ADC configuration for GPIO6 (ADC1_CHANNEL_5)
#define FUEL_ADC_CHANNEL        ADC1_CHANNEL_5  // GPIO6
#define FUEL_ADC_ATTEN          ADC_ATTEN_DB_12 // 0-3.3V range
#define FUEL_ADC_WIDTH          ADC_WIDTH_BIT_12

// Moving average filter configuration
#define MOVING_AVG_SIZE         8               // Reduced from 15 for less CPU usage
#define ADC_SAMPLES_PER_READ    8               // Reduced from 64 - much faster

static bool fuel_adc_initialized = false;

static float voltage_buffer[MOVING_AVG_SIZE];   // Circular buffer for moving average
static int buffer_index = 0;                   // Current position in buffer
static bool buffer_filled = false;             // Whether buffer has been filled once
static float last_stable_voltage = 0.0f;       // Last stable voltage reading

esp_err_t fuel_input_init(void)
{
    esp_err_t ret = ESP_OK;

    // Configure ADC width
    ret = adc1_config_width(FUEL_ADC_WIDTH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC width: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure ADC channel attenuation
    ret = adc1_config_channel_atten(FUEL_ADC_CHANNEL, FUEL_ADC_ATTEN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize moving average buffer
    memset(voltage_buffer, 0, sizeof(voltage_buffer));
    buffer_index = 0;
    buffer_filled = false;
    last_stable_voltage = 0.0f;

    fuel_adc_initialized = true;
    ESP_LOGI(TAG, "Fuel input initialized on GPIO6 (ADC1_CH5) with %d-sample moving average and advanced filtering", MOVING_AVG_SIZE);
    return ESP_OK;
}

esp_err_t fuel_input_deinit(void)
{
    fuel_adc_initialized = false;
    // Reset moving average state
    memset(voltage_buffer, 0, sizeof(voltage_buffer));
    buffer_index = 0;
    buffer_filled = false;
    last_stable_voltage = 0.0f;
    return ESP_OK;
}

float fuel_input_read_voltage(void)
{
    if (!fuel_adc_initialized) {
        ESP_LOGE(TAG, "Fuel ADC not initialized");
        return last_stable_voltage; // Return last known good value
    }

    // Take fewer readings for much better performance
    uint32_t adc_sum = 0;
    int successful_readings = 0;
    
    for (int i = 0; i < ADC_SAMPLES_PER_READ; i++) {
        int raw_value = adc1_get_raw(FUEL_ADC_CHANNEL);
        if (raw_value >= 0) {
            adc_sum += raw_value;
            successful_readings++;
        }
        // Removed delay - ADC is fast enough without it
    }

    if (successful_readings == 0) {
        ESP_LOGW(TAG, "Failed to read ADC, returning last stable voltage: %.3fV", last_stable_voltage);
        return last_stable_voltage;
    }

    uint32_t adc_average = adc_sum / successful_readings;
    
    // Convert raw reading to voltage manually
    // ESP32-S3 ADC is 12-bit (4095 max) with 12dB attenuation for 0-3.3V range
    float current_voltage = (adc_average * 3.3f) / 4095.0f;
    
    // Add current reading to moving average buffer
    voltage_buffer[buffer_index] = current_voltage;
    buffer_index = (buffer_index + 1) % MOVING_AVG_SIZE;
    
    if (!buffer_filled && buffer_index == 0) {
        buffer_filled = true;
    }
    
    // Calculate moving average
    float voltage_sum = 0.0f;
    int samples_to_average = buffer_filled ? MOVING_AVG_SIZE : buffer_index;
    
    for (int i = 0; i < samples_to_average; i++) {
        voltage_sum += voltage_buffer[i];
    }
    
    float averaged_voltage = voltage_sum / samples_to_average;
    
    // Simplified smoothing - less CPU intensive
    const float smoothing_factor = 0.4f; // Increased for faster response
    if (last_stable_voltage > 0.0f) {
        averaged_voltage = (smoothing_factor * averaged_voltage) + ((1.0f - smoothing_factor) * last_stable_voltage);
    }
    
    last_stable_voltage = averaged_voltage;
    
    return averaged_voltage;
}

float fuel_input_calculate_level(float voltage, float empty_voltage, float full_voltage)
{
    if (full_voltage <= empty_voltage) {
        ESP_LOGW(TAG, "Invalid calibration: full_voltage (%.3fV) <= empty_voltage (%.3fV)", 
                 full_voltage, empty_voltage);
        return 0.0f;
    }

    float level = (voltage - empty_voltage) / (full_voltage - empty_voltage);
    
    // Clamp to 0-100%
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    
    return level * 100.0f; // Return as percentage
}

// Debug function to log voltage readings and moving average state
void fuel_input_debug_readings(void)
{
    if (!fuel_adc_initialized) {
        ESP_LOGW(TAG, "Fuel ADC not initialized for debug");
        return;
    }
    
    // Get a fresh raw reading without filtering (reduced samples for performance)
    uint32_t adc_sum = 0;
    int successful_readings = 0;
    
    for (int i = 0; i < 4; i++) { // Reduced from 32 to 4 for performance
        int raw_value = adc1_get_raw(FUEL_ADC_CHANNEL);
        if (raw_value >= 0) {
            adc_sum += raw_value;
            successful_readings++;
        }
    }
    
    if (successful_readings > 0) {
        uint32_t raw_average = adc_sum / successful_readings;
        float raw_voltage = (raw_average * 3.3f) / 4095.0f;
        
        ESP_LOGI(TAG, "Debug - Raw: %luADC (%.3fV), Filtered: %.3fV, Buffer filled: %s", 
                 raw_average, raw_voltage, last_stable_voltage, buffer_filled ? "Yes" : "No");
        
        // Show moving average buffer contents (split for readability)
        ESP_LOGI(TAG, "Buffer contents: [%.3f, %.3f, %.3f, %.3f]",
                 voltage_buffer[0], voltage_buffer[1], voltage_buffer[2], voltage_buffer[3]);
        ESP_LOGI(TAG, "                  [%.3f, %.3f, %.3f, %.3f]",
                 voltage_buffer[4], voltage_buffer[5], voltage_buffer[6], voltage_buffer[7]);
    }
} 