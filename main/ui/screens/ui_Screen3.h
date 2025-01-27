#ifndef UI_SCREEN3_H
#define UI_SCREEN3_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void save_warning_configs_to_nvs();
extern void load_warning_configs_from_nvs();
extern void load_values_config_from_nvs();

extern lv_obj_t * g_label_input[];
extern lv_obj_t * g_can_id_input[];
extern lv_obj_t * g_endian_dropdown[];
extern lv_obj_t * g_bit_start_dropdown[];
extern lv_obj_t * g_bit_length_dropdown[];
extern lv_obj_t * g_scale_input[];
extern lv_obj_t * g_offset_input[];
extern lv_obj_t * g_decimals_dropdown[];
extern lv_obj_t * g_type_dropdown[];


typedef enum {
    BIG_ENDIAN_ORDER = 0,
    LITTLE_ENDIAN_ORDER = 1
} endian_t;

typedef struct {
    bool enabled;
    uint32_t can_id;
    uint8_t endianess;
    uint8_t bit_start;
    uint8_t bit_length;
    uint8_t decimals;
    float value_offset;
    float scale;
    int32_t bar_min;
    int32_t bar_max;
    bool is_signed;
    float warning_high_threshold;
    float warning_low_threshold;
    lv_color_t warning_high_color;
    lv_color_t warning_low_color;
    bool warning_high_enabled;
    bool warning_low_enabled;
    lv_color_t rpm_bar_color;
} value_config_t;

typedef struct {
    uint32_t can_id;    // CAN ID to monitor
    uint8_t bit_position;    // Which bit to check (0-63)
    lv_color_t active_color;  // Color when warning is active
    char label[32];    // Warning label text
    bool is_momentary;  // true for momentary, false for toggle
    bool current_state; // tracks current toggle state
} warning_config_t;

extern warning_config_t warning_configs[8];
extern value_config_t values_config[13];
extern uint8_t current_value_id;

extern char label_texts[13][64];
extern char value_offset_texts[13][64];

#ifdef __cplusplus
}
#endif

#endif // UI_SCREEN3_H
