#ifndef DEVICE_SETTINGS_H
#define DEVICE_SETTINGS_H

#include "lvgl.h"
#include <stdint.h>

// Public function declarations
void device_settings_longpress_cb(lv_event_t* e);
void device_settings_with_return_screen(lv_obj_t* return_screen);
void set_display_brightness(int percent);
void init_display_brightness(void);
void load_ecu_preconfig(void);
uint8_t get_selected_ecu_preconfig(void);
uint8_t get_selected_ecu_version(void);

#endif // DEVICE_SETTINGS_H 