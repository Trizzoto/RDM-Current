#ifndef LVGL_HELPERS_H
#define LVGL_HELPERS_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Declare the LVGL mutex as extern
extern SemaphoreHandle_t lvgl_mux;

// Function declarations
bool example_lvgl_lock(int timeout_ms);
void example_lvgl_unlock(void);

#endif // LVGL_HELPERS_H
