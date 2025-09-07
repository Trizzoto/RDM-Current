#ifndef FUEL_INPUT_H
#define FUEL_INPUT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the fuel input ADC
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t fuel_input_init(void);

/**
 * @brief Deinitialize the fuel input ADC
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t fuel_input_deinit(void);

/**
 * @brief Read the current voltage from the fuel sender
 * 
 * @return float Voltage in volts (0.0 - 3.3V)
 */
float fuel_input_read_voltage(void);

/**
 * @brief Calculate fuel level percentage from voltage
 * 
 * @param voltage Current voltage reading
 * @param empty_voltage Voltage when tank is empty
 * @param full_voltage Voltage when tank is full
 * @return float Fuel level percentage (0.0 - 100.0)
 */
float fuel_input_calculate_level(float voltage, float empty_voltage, float full_voltage);

/**
 * @brief Debug function to log voltage readings and filter state
 * 
 * Useful for troubleshooting voltage stability and filter performance
 */
void fuel_input_debug_readings(void);

#ifdef __cplusplus
}
#endif

#endif // FUEL_INPUT_H 