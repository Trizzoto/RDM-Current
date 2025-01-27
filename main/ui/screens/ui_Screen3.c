#include "core/lv_obj.h"
#include "core/lv_obj_pos.h"
#include "font/lv_font.h"
#include "misc/lv_area.h"
#include "misc/lv_color.h"
#include "ui.h"
#include "esp_err.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "ui_helpers.h"

// External CAN configurations
extern twai_timing_config_t g_t_config;
extern twai_general_config_t g_config;
extern twai_filter_config_t f_config;

#include <stdlib.h>
#include <time.h>
#include "ui_preconfig.h"
#include "screens/ui_Screen3.h"
#include <math.h>
#include "lvgl.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "ui.h"
#include "ui_helpers.h"

/////////////////////////////////////////////	DEFINE ARRAYS and STRUCTURES	/////////////////////////////////////////////

static lv_obj_t * ui_Label[13] = {NULL};
static lv_obj_t * ui_Value[13] = {NULL};
static lv_obj_t * ui_Box[8] = {NULL};
static lv_obj_t * ui_MenuScreen = NULL;
static lv_obj_t * keyboard = NULL; // Global keyboard object
static lv_obj_t * rpm_bar_gauge = NULL;
lv_obj_t * ui_Gear_Label = NULL;
int rpm_gauge_max = 7000; // Default max RPM value

#define RPM_VALUE_ID 9
#define SPEED_VALUE_ID 10
#define GEAR_VALUE_ID 11
#define BAR1_VALUE_ID 12
#define BAR2_VALUE_ID 13

value_config_t values_config[13];
uint8_t current_value_id;

extern void save_values_config_to_nvs();

#define MAX_VALUES 13

 lv_obj_t * g_label_input[MAX_VALUES];
 lv_obj_t * g_can_id_input[MAX_VALUES];
 lv_obj_t * g_endian_dropdown[MAX_VALUES];
 lv_obj_t * g_bit_start_dropdown[MAX_VALUES];
 lv_obj_t * g_bit_length_dropdown[MAX_VALUES];
 lv_obj_t * g_scale_input[MAX_VALUES];
 lv_obj_t * g_offset_input[MAX_VALUES];
 lv_obj_t * g_decimals_dropdown[MAX_VALUES];
 lv_obj_t * g_type_dropdown[MAX_VALUES];

// Text buffers for labels, units, endianess and value offsets
char label_texts[13][64] = {"PANEL 1","PANEL 2","PANEL 3","PANEL 4","PANEL 5","PANEL 6","PANEL 7","PANEL 8", "RPM", "SPEED", "GEAR", "BAR 1", "BAR 2"};  // default texts
char value_offset_texts[13][64] = {"0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0"};  // default offsets
uint8_t endianess[13] = {1}; //Storage for endianness: 0 = Big Endian, 1 = Small Endian

// Positions arrays
static const lv_coord_t label_positions[8][2] = {
    {-312, -54}, {-146, -54}, {-312, 54}, {-146, 54},
    {146, -54}, {312, -54}, {146, 54}, {312, 54}
};
static const lv_coord_t value_positions[8][2] = {
    {-312, -17}, {-146, -17}, {-312, 91}, {-146, 91},
    {146, -17}, {312, -17}, {146, 91}, {312, 91}
};
static const lv_coord_t box_positions[8][2] = {
    {-312, -26}, {-146, -26}, {-312, 82}, {-146, 82},
    {146, -26}, {312, -26}, {146, 82}, {312, 82}
};

// Define positions for warning circles and labels
static const struct {
    int16_t x;
    int16_t y;
} warning_positions[] = {
    {-352, -148},  // Warning 1
    {-292, -148},  // Warning 2
    {-232, -148},  // Warning 3
    {-172, -148},  // Warning 4
    {172, -148},   // Warning 5
    {232, -148},   // Warning 6
    {292, -148},   // Warning 7
    {352, -148}    // Warning 8
};

warning_config_t warning_configs[8];
static lv_obj_t* warning_circles[8] = {NULL};
static lv_obj_t* warning_labels[8] = {NULL};
static uint64_t last_signal_times[8] = {0};
static bool toggle_debounce[8] = {false};
static uint64_t toggle_start_time[8] = {0};

typedef struct {
    uint8_t warning_idx;
    lv_obj_t** input_objects;
    lv_obj_t** preview_objects;
} warning_save_data_t;

void init_values_config_defaults(void) {
    for(int i = 0; i < 13; i++) {
        values_config[i].enabled = false;
        values_config[i].can_id = 0;
        values_config[i].endianess = 0;
        values_config[i].bit_start = 0;
        values_config[i].bit_length = 0;
        values_config[i].decimals = 0;
        values_config[i].value_offset = 0;
        values_config[i].scale = 1;
        values_config[i].is_signed = false;
      
		if (i < 8) {
		values_config[i].warning_high_threshold = 0;
        values_config[i].warning_low_threshold = 0;
        values_config[i].warning_high_color = lv_color_hex(0xFF0000); // Default red
        values_config[i].warning_low_color = lv_color_hex(0x19439a); // Default blue
        values_config[i].warning_high_enabled = false;
        values_config[i].warning_low_enabled = false;
        }
    }
  
    values_config[RPM_VALUE_ID - 1].enabled = true;
    values_config[RPM_VALUE_ID - 1].rpm_bar_color = lv_color_hex(0xFF0000); // Default red
    values_config[SPEED_VALUE_ID - 1].enabled = true;
    values_config[GEAR_VALUE_ID - 1].enabled = true;
    values_config[BAR1_VALUE_ID - 1].enabled = true;
    values_config[BAR2_VALUE_ID - 1].enabled = true;
  
    // Set default ranges for bars
    values_config[BAR1_VALUE_ID - 1].bar_min = 0;
    values_config[BAR1_VALUE_ID - 1].bar_max = 100;
    values_config[BAR2_VALUE_ID - 1].bar_min = 0;
    values_config[BAR2_VALUE_ID - 1].bar_max = 100;
}

static void init_warning_configs(void) {
    for(int i = 0; i < 8; i++) {
        warning_configs[i].can_id = 0;
        warning_configs[i].bit_position = 0;
        warning_configs[i].active_color = lv_color_hex(0xFF0000);  // Red color
        warning_configs[i].is_momentary = true;  // Default to momentary mode
        warning_configs[i].current_state = false;
        snprintf(warning_configs[i].label, sizeof(warning_configs[i].label), 
            "Warning %d", i + 1);
    }
}

static void print_value_config(uint8_t value_id) {
    uint8_t idx = value_id - 1;
    printf("Value #%d Configuration:\n", value_id);
    printf("  Enabled: %s\n", values_config[idx].enabled ? "Yes" : "No");
    printf("  CAN ID: %u\n", values_config[idx].can_id);
    printf("  Endianess: %s\n", values_config[idx].endianess == 0 ? "Big Endian" : "Little Endian");
    printf("  Bit Start: %d\n", values_config[idx].bit_start);
    printf("  Bit Length: %d\n", values_config[idx].bit_length);
    printf("  Decimals: %d\n", values_config[idx].decimals);
    printf("  Value Offset: %g\n", values_config[idx].value_offset);
    printf("  Scale: %g\n", values_config[idx].scale);
    printf("-------------------------------------------\n");
}

/////////////////////////////////////////////	FORWARD DECLERATIONS	/////////////////////////////////////////////

static void load_menu_screen_for_value(uint8_t value_id);
static void close_menu_event_cb(lv_event_t * e);
static void create_menu_objects(lv_obj_t * parent, uint8_t value_id);
static void create_config_controls(lv_obj_t * parent, uint8_t value_id);
static void keyboard_event_cb(lv_event_t * e);
static void label_input_event_cb(lv_event_t * e);
static void value_long_press_event_cb(lv_event_t * e);
static void free_value_id_event_cb(lv_event_t * e);
static void keyboard_ready_event_cb(lv_event_t * e);
static void rpm_color_dropdown_event_cb(lv_event_t * e);
static void device_settings_longpress_cb(lv_event_t* e);
static void wifi_settings_cb(lv_event_t* e);
static void wifi_switch_event_cb(lv_event_t * e);
void update_rpm_lines(lv_obj_t *parent);
void set_rpm_value(int rpm);
void create_warning_config_menu(uint8_t warning_idx);


static bool rpm_color_needs_update = false;
static lv_color_t new_rpm_color;
static char previous_values[13][64] = {0};
static float previous_bar_values[2] = {0, 0};
static uint32_t last_long_press_time = 0;
static char value_str_buffer[64];


#define BAR_UPDATE_THRESHOLD 1.0
#define LONG_PRESS_COOLDOWN 500
#define LABEL_TEXT_MAX_LEN 32
																												
/////////////////////////////////////////////	CALLBACKS	/////////////////////////////////////////////

static void label_input_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * textarea = lv_event_get_target(e);
        const char * txt = lv_textarea_get_text(textarea);
        if (txt == NULL) return;

        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);

        // Handle regular panels (1-8) and gear
        if ((value_id >= 1 && value_id <= 8) || (value_id == GEAR_VALUE_ID)) {
            strncpy(label_texts[value_id - 1], txt, sizeof(label_texts[value_id - 1]));
            label_texts[value_id - 1][sizeof(label_texts[value_id - 1]) - 1] = '\0';

            if(value_id == GEAR_VALUE_ID && ui_Gear_Label) {
                lv_label_set_text(ui_Gear_Label, label_texts[value_id - 1]);
            } else if (ui_Label[value_id - 1]) {
                lv_label_set_text(ui_Label[value_id - 1], label_texts[value_id - 1]);
            }
        }
        // Handle bar labels
        else if (value_id == BAR1_VALUE_ID && ui_Bar_1_Label) {
            strncpy(label_texts[value_id - 1], txt, sizeof(label_texts[value_id - 1]));
            label_texts[value_id - 1][sizeof(label_texts[value_id - 1]) - 1] = '\0';
            lv_label_set_text(ui_Bar_1_Label, txt);
        }
        else if (value_id == BAR2_VALUE_ID && ui_Bar_2_Label) {
            strncpy(label_texts[value_id - 1], txt, sizeof(label_texts[value_id - 1]));
            label_texts[value_id - 1][sizeof(label_texts[value_id - 1]) - 1] = '\0';
            lv_label_set_text(ui_Bar_2_Label, txt);
        }
    }
}


static void bit_start_roller_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * roller = lv_event_get_target(e);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);

        if (value_id < 1 || value_id > 13) return;

        // Get the selected option index
        uint8_t selected_bit_start = lv_dropdown_get_selected(roller);
        values_config[value_id - 1].bit_start = selected_bit_start;

        printf("Updated Bit Start for Value #%d to %d\n", value_id, selected_bit_start);
        print_value_config(value_id);
    }
}


static void bit_length_roller_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * roller = lv_event_get_target(e);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);

        if (value_id < 1 || value_id > 13) return;

        // Get the selected option index and adjust for 1-based length
        uint8_t selected_bit_length = lv_dropdown_get_selected(roller) + 1;
        values_config[value_id - 1].bit_length = selected_bit_length;

        printf("Updated Bit Length for Value #%d to %d\n", value_id, selected_bit_length);
        print_value_config(value_id);
    }
}

static void decimal_dropdown_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t * dropdown = lv_event_get_target(e);

    // Get which value_id we're editing
    uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
    if(value_id < 1 || value_id > 13) return;

    // 0 => "0", 1 => "1", 2 => "2"
    uint8_t selected = lv_dropdown_get_selected(dropdown);
    values_config[value_id - 1].decimals = selected;

    printf("Updated Decimals for Value #%d to %d\n", value_id, selected);
    print_value_config(value_id);
}

static void delete_old_screen_cb(lv_timer_t * timer) {
    lv_obj_t * screen = (lv_obj_t *)timer->user_data;
    if (screen) {
        destroy_preconfig_menu();
        lv_obj_del(screen);
    }
}

static void close_menu_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    // Save configuration first, while UI elements are still valid
    save_values_config_to_nvs();

    // Store current screen for cleanup
    lv_obj_t * old_screen = lv_scr_act();

    // Create and initialize new screen before deleting old one
    ui_Screen3 = lv_obj_create(NULL);
    if (!ui_Screen3) return;

    // Initialize the new screen
    ui_Screen3_screen_init();

    // Load new screen with fade animation
    lv_scr_load_anim(ui_Screen3, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);

    // Clean up old screen after a short delay to ensure smooth transition
    lv_timer_t * del_timer = lv_timer_create(delete_old_screen_cb, 200, old_screen);
    lv_timer_set_repeat_count(del_timer, 1);
}


// Show keyboard for text input focus
static void keyboard_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_FOCUSED) {
        if(obj == NULL) return;
        lv_keyboard_set_textarea(keyboard, obj);
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    } else if(code == LV_EVENT_DEFOCUSED) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void keyboard_ready_event_cb(lv_event_t * e) {
    lv_obj_t * keyboard = lv_event_get_target(e);
    // Hide the keyboard
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void value_long_press_event_cb(lv_event_t * e) {
    uint32_t current_time = lv_tick_get();
    if (current_time - last_long_press_time < LONG_PRESS_COOLDOWN) return;
    last_long_press_time = current_time;

    uint8_t * p_id = (uint8_t *)lv_event_get_user_data(e);
    if (!p_id) return;

    current_value_id = *p_id;
    printf("Long press detected on value %u\n", current_value_id);
    load_menu_screen_for_value(current_value_id);
}

static void free_value_id_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        uint8_t * p_id = (uint8_t *)lv_event_get_user_data(e);
        if (p_id) {
            lv_mem_free(p_id);  // Only free if it's dynamically allocated
        }
    }
}


static void endianess_roller_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = lv_event_get_target(e);
        uint8_t selected = lv_dropdown_get_selected(dropdown);

        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
        if (value_id < 1 || value_id > 13) return;

        // Update the configuration
        values_config[value_id - 1].endianess = (selected == 0) ? BIG_ENDIAN_ORDER : LITTLE_ENDIAN_ORDER;

        printf("Updated Endianess for Value #%d to %s\n",
               value_id,
               (selected == 0) ? "Big Endian" : "Little Endian");
        print_value_config(value_id);
    }
}


static void value_offset_input_event_cb(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * textarea = lv_event_get_target(e);
        const char * txt = lv_textarea_get_text(textarea);

        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
        if(value_id < 1 || value_id > 13) return;

        strncpy(value_offset_texts[value_id -1], txt, sizeof(value_offset_texts[value_id -1]));
        value_offset_texts[value_id -1][sizeof(value_offset_texts[value_id -1]) - 1] = '\0'; 

        float entered_offset = atof(txt);
        values_config[value_id - 1].value_offset = entered_offset;

        printf("Updated Value Offset for Value #%d to %f\n", value_id, entered_offset);
        print_value_config(value_id);
    }
}


static void can_id_input_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * textarea = lv_event_get_target(e);
        const char * txt = lv_textarea_get_text(textarea);

        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
        if(value_id < 1 || value_id > 13) return;

        // 1) Build a "0x..." string from what the user typed
        //    If user typed "0x123", skip re-appending "0x"
        char buffer[32];
        if(txt != NULL && txt[0] != '\0') {
            if((txt[0] == '0') && (txt[1] == 'x' || txt[1] == 'X')) {
                // Already starts with 0x, copy as-is
                snprintf(buffer, sizeof(buffer), "%s", txt);
            } else {
                // Prepend 0x
                snprintf(buffer, sizeof(buffer), "0x%s", txt);
            }
        } else {
            // Empty text means ID=0
            snprintf(buffer, sizeof(buffer), "0x0");
        }

        // 2) Convert to number, always as hex
        uint32_t entered_can_id = strtoul(buffer, NULL, 16);

        // 3) Store the numeric ID in your config
        values_config[value_id - 1].can_id = entered_can_id;
        values_config[value_id - 1].enabled = (entered_can_id != 0);

        printf("Updated CAN ID for Value #%d to 0x%X (decimal: %u)\n",
               value_id, entered_can_id, entered_can_id);
        print_value_config(value_id);
    }
}


static void scale_input_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * textarea = lv_event_get_target(e);
        const char * txt = lv_textarea_get_text(textarea);

        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
        if(value_id < 1 || value_id > 13) return;

        float entered_scale = atof(txt);
        if (entered_scale == 0.0f) {
            entered_scale = 1.0f;
        }
        values_config[value_id - 1].scale = entered_scale;
        printf("Updated Scale for Value #%d to %f\n", value_id, entered_scale);
        print_value_config(value_id);
    }
}

static void rpm_gauge_roller_event_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    uint16_t selected = lv_roller_get_selected(roller);
    rpm_gauge_max = 3000 + (selected * 1000); //scale from 3000 to 12000
	update_rpm_lines(lv_scr_act());
}

static void rpm_ecu_dropdown_event_cb(lv_event_t * e)
{
    lv_obj_t * dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    // Indices: 0 = Custom, 1 = MaxxECU, 2 = Haltech

    if (selected == 0) {
        // ========== CUSTOM ==========
        printf("ECU Presets: Custom (no changes)\n");
        // Do nothing, or set defaults if you prefer
    }
    else if (selected == 1) {
        // ========== MAXXECU ==========
        printf("ECU Presets: MaxxECU\n");
        values_config[RPM_VALUE_ID - 1].can_id       = 520;  // 0x208
        values_config[RPM_VALUE_ID - 1].endianess    = 1;    // 0=Big,1=Little (check your usage)
        values_config[RPM_VALUE_ID - 1].bit_start    = 0;
        values_config[RPM_VALUE_ID - 1].bit_length   = 16;
        values_config[RPM_VALUE_ID - 1].scale        = 1.0f;
        values_config[RPM_VALUE_ID - 1].value_offset = 0.0f;
        values_config[RPM_VALUE_ID - 1].decimals     = 0;

        // Now update the UI fields for RPM (ID=9 => index=8)
        // --------------------------------------------------
        if (g_can_id_input[RPM_VALUE_ID - 1]) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%u", values_config[RPM_VALUE_ID - 1].can_id);
            lv_textarea_set_text(g_can_id_input[RPM_VALUE_ID - 1], buf);
        }

        // Endianness: your dropdown presumably has "Big Endian\nLittle Endian"
        // 0 => Big, 1 => Little
        if (g_endian_dropdown[RPM_VALUE_ID - 1]) {
            lv_dropdown_set_selected(g_endian_dropdown[RPM_VALUE_ID - 1],
                                     values_config[RPM_VALUE_ID - 1].endianess);
        }

        // Bit start
        if (g_bit_start_dropdown[RPM_VALUE_ID - 1]) {
            lv_dropdown_set_selected(g_bit_start_dropdown[RPM_VALUE_ID - 1],
                                     values_config[RPM_VALUE_ID - 1].bit_start);
        }

        // Bit length (the dropdown might list 1..64, so we subtract 1 for zero-based index)
        if (g_bit_length_dropdown[RPM_VALUE_ID - 1]) {
            lv_dropdown_set_selected(g_bit_length_dropdown[RPM_VALUE_ID - 1],
                                     values_config[RPM_VALUE_ID - 1].bit_length - 1);
        }

        // Scale
        if (g_scale_input[RPM_VALUE_ID - 1]) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%g", values_config[RPM_VALUE_ID - 1].scale);
            lv_textarea_set_text(g_scale_input[RPM_VALUE_ID - 1], buf);
        }

        // Value offset
        if (g_offset_input[RPM_VALUE_ID - 1]) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%g", values_config[RPM_VALUE_ID - 1].value_offset);
            lv_textarea_set_text(g_offset_input[RPM_VALUE_ID - 1], buf);
        }

        // Decimals
        if (g_decimals_dropdown[RPM_VALUE_ID - 1]) {
            lv_dropdown_set_selected(g_decimals_dropdown[RPM_VALUE_ID - 1],
                                     values_config[RPM_VALUE_ID - 1].decimals);
        }
    }
    else if (selected == 2) {
        // ========== HALTECH ==========
        printf("ECU Presets: Haltech\n");
        values_config[RPM_VALUE_ID - 1].can_id       = 521;  // 0x209
        values_config[RPM_VALUE_ID - 1].endianess    = 1;
        values_config[RPM_VALUE_ID - 1].bit_start    = 0;
        values_config[RPM_VALUE_ID - 1].bit_length   = 16;
        values_config[RPM_VALUE_ID - 1].scale        = 1.0f;
        values_config[RPM_VALUE_ID - 1].value_offset = 0.0f;
        values_config[RPM_VALUE_ID - 1].decimals     = 0;

        // Update UI fields similarly
        // --------------------------------------------------
        if (g_can_id_input[RPM_VALUE_ID - 1]) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%u", values_config[RPM_VALUE_ID - 1].can_id);
            lv_textarea_set_text(g_can_id_input[RPM_VALUE_ID - 1], buf);
        }

        if (g_endian_dropdown[RPM_VALUE_ID - 1]) {
            lv_dropdown_set_selected(g_endian_dropdown[RPM_VALUE_ID - 1],
                                     values_config[RPM_VALUE_ID - 1].endianess);
        }

        if (g_bit_start_dropdown[RPM_VALUE_ID - 1]) {
            lv_dropdown_set_selected(g_bit_start_dropdown[RPM_VALUE_ID - 1],
                                     values_config[RPM_VALUE_ID - 1].bit_start);
        }

        if (g_bit_length_dropdown[RPM_VALUE_ID - 1]) {
            lv_dropdown_set_selected(g_bit_length_dropdown[RPM_VALUE_ID - 1],
                                     values_config[RPM_VALUE_ID - 1].bit_length - 1);
        }

        if (g_scale_input[RPM_VALUE_ID - 1]) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%g", values_config[RPM_VALUE_ID - 1].scale);
            lv_textarea_set_text(g_scale_input[RPM_VALUE_ID - 1], buf);
        }

        if (g_offset_input[RPM_VALUE_ID - 1]) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%g", values_config[RPM_VALUE_ID - 1].value_offset);
            lv_textarea_set_text(g_offset_input[RPM_VALUE_ID - 1], buf);
        }

        if (g_decimals_dropdown[RPM_VALUE_ID - 1]) {
            lv_dropdown_set_selected(g_decimals_dropdown[RPM_VALUE_ID - 1],
                                     values_config[RPM_VALUE_ID - 1].decimals);
        }
    }

    // Print config to confirm
    print_value_config(RPM_VALUE_ID);
}

static void bar_range_input_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * textarea = lv_event_get_target(e);
        const char * txt = lv_textarea_get_text(textarea);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
      
        bool is_min = lv_obj_get_user_data(textarea) != NULL; // Check if this is min input
        int32_t value = atoi(txt);

        if (value_id == BAR1_VALUE_ID) {
            if (is_min) {
                values_config[value_id - 1].bar_min = value;
                lv_bar_set_range(ui_Bar_1, value, values_config[value_id - 1].bar_max);
            } else {
                values_config[value_id - 1].bar_max = value;
                lv_bar_set_range(ui_Bar_1, values_config[value_id - 1].bar_min, value);
            }
        }
        else if (value_id == BAR2_VALUE_ID) {
            if (is_min) {
                values_config[value_id - 1].bar_min = value;
                lv_bar_set_range(ui_Bar_2, value, values_config[value_id - 1].bar_max);
            } else {
                values_config[value_id - 1].bar_max = value;
                lv_bar_set_range(ui_Bar_2, values_config[value_id - 1].bar_min, value);
            }
        }
    }
}

static void type_dropdown_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = lv_event_get_target(e);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
        if(value_id < 1 || value_id > 13) return;

        uint8_t selected = lv_dropdown_get_selected(dropdown);
        values_config[value_id - 1].is_signed = (selected == 1);

        printf("Updated Type for Value #%d to %s\n", value_id, 
               values_config[value_id - 1].is_signed ? "Signed" : "Unsigned");
        print_value_config(value_id);
    }
}

static void rpm_color_dropdown_event_cb(lv_event_t * e) {
    lv_obj_t * dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
  
    // Determine new color based on selection
    switch(selected) {
        case 0: new_rpm_color = lv_color_hex(0x38FF00); break; // Green
        case 1: new_rpm_color = lv_color_hex(0x00E1FF); break; // Light Blue
        case 2: new_rpm_color = lv_color_hex(0xFFEB3B); break; // Yellow
        case 3: new_rpm_color = lv_color_hex(0xFF9800); break; // Orange
        case 4: new_rpm_color = lv_color_hex(0xFF0000); break; // Red
        case 5: new_rpm_color = lv_color_hex(0x0000FF); break; // Dark Blue
        case 6: new_rpm_color = lv_color_hex(0x9C27B0); break; // Purple
        case 7: new_rpm_color = lv_color_hex(0xFF00FF); break; // Magenta
        case 8: new_rpm_color = lv_color_hex(0xFF4081); break; // Pink
        default: new_rpm_color = lv_color_hex(0x38FF00); break;
    }
  
    rpm_color_needs_update = true;
    values_config[RPM_VALUE_ID - 1].rpm_bar_color = new_rpm_color;
}

static void check_rpm_color_update(lv_timer_t * timer) {
    if (rpm_color_needs_update) {
        if (rpm_bar_gauge) {
            lv_obj_set_style_bg_color(rpm_bar_gauge, new_rpm_color, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        if (ui_Panel9) {
            lv_obj_set_style_bg_color(ui_Panel9, new_rpm_color, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        rpm_color_needs_update = false;
    }
}

static void warning_high_threshold_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * textarea = lv_event_get_target(e);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
        const char * txt = lv_textarea_get_text(textarea);
        values_config[value_id - 1].warning_high_threshold = atof(txt);
        values_config[value_id - 1].warning_high_enabled = true;
    }
}

static void warning_low_threshold_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * textarea = lv_event_get_target(e);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
        const char * txt = lv_textarea_get_text(textarea);
        values_config[value_id - 1].warning_low_threshold = atof(txt);
        values_config[value_id - 1].warning_low_enabled = true;
    }
}

static void warning_high_color_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = lv_event_get_target(e);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
        uint16_t selected = lv_dropdown_get_selected(dropdown);
        values_config[value_id - 1].warning_high_color = selected == 0 ? 
            lv_color_hex(0xFF0000) : lv_color_hex(0x19439a);
    }
}

static void warning_low_color_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = lv_event_get_target(e);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);
        uint16_t selected = lv_dropdown_get_selected(dropdown);
        values_config[value_id - 1].warning_low_color = selected == 0 ? 
            lv_color_hex(0xFF0000) : lv_color_hex(0x19439a);
    }
}

static void warning_longpress_cb(lv_event_t* e) {
    uint8_t warning_idx = *(uint8_t*)lv_event_get_user_data(e);
    create_warning_config_menu(warning_idx);
}

static void label_text_cb(lv_event_t* e) {
    lv_obj_t* textarea = lv_event_get_target(e);
    warning_save_data_t* data = (warning_save_data_t*)lv_event_get_user_data(e);
    const char* txt = lv_textarea_get_text(textarea);
    if (data->preview_objects && data->preview_objects[1]) {
        lv_label_set_text(data->preview_objects[1], txt);
    }
}

static void color_dropdown_cb(lv_event_t* e) {
    lv_obj_t* dropdown = lv_event_get_target(e);
    warning_save_data_t* data = (warning_save_data_t*)lv_event_get_user_data(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    lv_color_t color;
    switch (selected) {
        case 0: color = lv_color_hex(0x00FF00); break; // Green
        case 1: color = lv_color_hex(0x0000FF); break; // Blue
        case 2: color = lv_color_hex(0xFFA500); break; // Orange
        case 3: color = lv_color_hex(0xFF0000); break; // Red
        case 4: color = lv_color_hex(0xFFFF00); break; // Yellow
        default: color = lv_color_hex(0x00FF00); break;
    }
    if (data->preview_objects && data->preview_objects[0]) {
        lv_obj_set_style_bg_color(data->preview_objects[0], color, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void save_warning_config_cb(lv_event_t* e) {
    warning_save_data_t* save_data = (warning_save_data_t*)lv_event_get_user_data(e);
    if (!save_data) {
        printf("Error: Invalid save data\n");
        return;
    }

    uint8_t warning_idx = save_data->warning_idx;
    lv_obj_t** inputs = save_data->input_objects;

    if (!inputs) {
        printf("Error: Invalid input objects\n");
        lv_mem_free(save_data);
        return;
    }

    // Get values from inputs
    const char* can_id_text = lv_textarea_get_text(inputs[0]);
    uint8_t bit_pos = lv_dropdown_get_selected(inputs[1]);
    const char* label_text = lv_textarea_get_text(inputs[3]);

    // Convert CAN ID from hex string to integer
    uint32_t can_id = 0;
    if (can_id_text && *can_id_text) {
        if (strncmp(can_id_text, "0x", 2) == 0) {
            sscanf(can_id_text + 2, "%x", &can_id);
        } else {
            sscanf(can_id_text, "%x", &can_id);
        }
    }

    // Update warning configuration
    warning_configs[warning_idx].can_id = can_id;
    warning_configs[warning_idx].bit_position = bit_pos;
    if (label_text) {
        strncpy(warning_configs[warning_idx].label, label_text, sizeof(warning_configs[warning_idx].label) - 1);
        warning_configs[warning_idx].label[sizeof(warning_configs[warning_idx].label) - 1] = '\0';
    }

    // Handle highlighted color selection
    if (inputs[4]) {
        uint8_t selected_color = lv_dropdown_get_selected(inputs[4]);
        switch (selected_color) {
            case 0: warning_configs[warning_idx].active_color = lv_color_hex(0x00FF00); break; // Green
            case 1: warning_configs[warning_idx].active_color = lv_color_hex(0x0000FF); break; // Blue
            case 2: warning_configs[warning_idx].active_color = lv_color_hex(0xFFA500); break; // Orange
            case 3: warning_configs[warning_idx].active_color = lv_color_hex(0xFF0000); break; // Red
            case 4: warning_configs[warning_idx].active_color = lv_color_hex(0xFFFF00); break; // Yellow
            default: warning_configs[warning_idx].active_color = lv_color_hex(0x00FF00); break;
        }
    }

    // Save toggle mode setting
    if (inputs[5]) {
        warning_configs[warning_idx].is_momentary = (lv_dropdown_get_selected(inputs[5]) == 1);
        warning_configs[warning_idx].current_state = false; // Reset state when changing modes
    }

    // Add callbacks for live preview updates
    lv_obj_add_event_cb(inputs[3], label_text_cb, LV_EVENT_VALUE_CHANGED, save_data);
    lv_obj_add_event_cb(inputs[4], color_dropdown_cb, LV_EVENT_VALUE_CHANGED, save_data);

    // Debug output
    printf("Warning %d configuration saved:\n", warning_idx + 1);
    printf("  CAN ID: 0x%X\n", can_id);
    printf("  Bit Position: %d\n", bit_pos);
    printf("  Label: %s\n", label_text ? label_text : "");
    printf("  Highlight Color: %06X\n", warning_configs[warning_idx].active_color.full);
    printf("  Mode: %s\n", warning_configs[warning_idx].is_momentary ? "Momentary" : "Toggle");

    // Update the label on Screen3 dynamically
    if (warning_labels[warning_idx]) {
        lv_label_set_text(warning_labels[warning_idx], warning_configs[warning_idx].label);
    }

    // Clean up
    lv_mem_free(inputs);
    lv_mem_free(save_data);
    save_warning_configs_to_nvs();

    // Return to Screen3
    lv_scr_load(ui_Screen3);
}

static void check_warning_timeouts(lv_timer_t * timer) {
    uint64_t current_time = esp_timer_get_time() / 1000; // Current time in milliseconds
    
    for (int i = 0; i < 8; i++) {
        if (warning_configs[i].is_momentary) {
            // Check if we've exceeded the 250ms timeout
            if (current_time - last_signal_times[i] > 500) {
                if (warning_configs[i].current_state) {  // Only update if currently active
                    warning_configs[i].current_state = false;
                    
                    // Update both circle color and label visibility
                    if (warning_circles[i]) {
                        lv_obj_set_style_bg_color(warning_circles[i], 
                            lv_color_hex(0x292C29), 
                            LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    
                    if (warning_labels[i]) {
                        lv_obj_add_flag(warning_labels[i], LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }
        }
    }
}

static void bitrate_dropdown_event_cb(lv_event_t * e) {
    lv_obj_t * dd = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dd);
    
    // Configure CAN timing based on selection
    switch(selected) {
        case 0: // 125 kbps
            g_t_config.brp = 64;
            g_t_config.tseg_1 = 15;
            g_t_config.tseg_2 = 4;
            g_t_config.sjw = 3;
            break;
        case 1: // 250 kbps
            g_t_config.brp = 32;
            g_t_config.tseg_1 = 15;
            g_t_config.tseg_2 = 4;
            g_t_config.sjw = 3;
            break;
        case 2: // 500 kbps
            g_t_config.brp = 16;
            g_t_config.tseg_1 = 15;
            g_t_config.tseg_2 = 4;
            g_t_config.sjw = 3;
            break;
        case 3: // 1 Mbps
            g_t_config.brp = 8;
            g_t_config.tseg_1 = 15;
            g_t_config.tseg_2 = 4;
            g_t_config.sjw = 3;
            break;
        default:
            return;
    }
    
    // Stop CAN driver, reconfigure, and restart
    twai_stop();
    twai_driver_uninstall();
    twai_driver_install(&g_config, &g_t_config, &f_config);
    twai_start();
}


static void device_settings_longpress_cb(lv_event_t* e) {
    lv_obj_t* old_screen = lv_scr_act();
    lv_obj_t* settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(settings_screen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Create a close button
    lv_obj_t* close_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(close_btn, 70, 50);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_t* close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "X");
    lv_obj_center(close_label);
    lv_obj_add_event_cb(close_btn, close_menu_event_cb, LV_EVENT_CLICKED, old_screen);

    // Add title
    lv_obj_t* title = lv_label_create(settings_screen);
    lv_label_set_text(title, "Device Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Create bitrate label
    lv_obj_t* bitrate_label = lv_label_create(settings_screen);
    lv_label_set_text(bitrate_label, "Bitrate:");
    lv_obj_align(bitrate_label, LV_ALIGN_TOP_LEFT, 40, 100);
    lv_obj_set_style_text_font(bitrate_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Create bitrate dropdown
    lv_obj_t* bitrate_dd = lv_dropdown_create(settings_screen);
    lv_dropdown_set_options(bitrate_dd, 
        "125 kbps\n"
        "250 kbps\n"
        "500 kbps\n"
        "1 Mbps");
    lv_obj_set_size(bitrate_dd, 150, LV_SIZE_CONTENT);
    lv_obj_align_to(bitrate_dd, bitrate_label, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_obj_add_event_cb(bitrate_dd, bitrate_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Set current bitrate in dropdown
    lv_dropdown_set_selected(bitrate_dd, 2); // Default to 500 kbps

    // Add Serial Number label
    lv_obj_t* serial_label = lv_label_create(settings_screen);
    lv_label_set_text(serial_label, "Serial No:");
    lv_obj_align(serial_label, LV_ALIGN_TOP_LEFT, 40, 160);
    lv_obj_set_style_text_font(serial_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(serial_label, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Add Serial Number value
    lv_obj_t* serial_value = lv_label_create(settings_screen);
    lv_label_set_text(serial_value, "RDM7ESP01");
    lv_obj_align_to(serial_value, serial_label, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_obj_set_style_text_font(serial_value, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(serial_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Add WiFi Settings button
    lv_obj_t* wifi_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(wifi_btn, 200, 50);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_LEFT, 40, 220);
    lv_obj_t* wifi_label = lv_label_create(wifi_btn);
    lv_label_set_text(wifi_label, "Wi-Fi Settings");
    lv_obj_center(wifi_label);
    lv_obj_add_event_cb(wifi_btn, wifi_settings_cb, LV_EVENT_CLICKED, NULL);

    // Add Check for Updates button
    lv_obj_t* update_btn = lv_btn_create(settings_screen);
    lv_obj_set_size(update_btn, 200, 50);
    lv_obj_align(update_btn, LV_ALIGN_TOP_LEFT, 40, 290);
    lv_obj_t* update_label = lv_label_create(update_btn);
    lv_label_set_text(update_label, "Check for Updates");
    lv_obj_center(update_label);

    lv_scr_load_anim(settings_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

// Add WiFi menu callback
static void wifi_settings_cb(lv_event_t* e) {
    lv_obj_t* old_screen = lv_scr_act();
    lv_obj_t* wifi_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_screen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(wifi_screen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Create a close button
    lv_obj_t* close_btn = lv_btn_create(wifi_screen);
    lv_obj_set_size(close_btn, 70, 50);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_t* close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "X");
    lv_obj_center(close_label);
    lv_obj_add_event_cb(close_btn, close_menu_event_cb, LV_EVENT_CLICKED, old_screen);

    // Add title
    lv_obj_t* title = lv_label_create(wifi_screen);
    lv_label_set_text(title, "Wi-Fi Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Add WiFi switch
    lv_obj_t* wifi_label = lv_label_create(wifi_screen);
    lv_label_set_text(wifi_label, "Wi-Fi:");
    lv_obj_align(wifi_label, LV_ALIGN_TOP_LEFT, 40, 100);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* wifi_switch = lv_switch_create(wifi_screen);
    lv_obj_align_to(wifi_switch, wifi_label, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_obj_add_event_cb(wifi_switch, wifi_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_scr_load(wifi_screen);
}

// Add WiFi switch callback
static void wifi_switch_event_cb(lv_event_t * e) {
    lv_obj_t * sw = lv_event_get_target(e);
    bool wifi_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    if (wifi_on) {
        // TODO: Turn on WiFi
    } else {
        // TODO: Turn off WiFi
    }
}

/////////////////////////////////////////////	PROCESSING	/////////////////////////////////////////////

void set_rpm_value(int rpm) {
    if (rpm < 0) rpm = 0;
    if (rpm_bar_gauge) {
        lv_bar_set_value(rpm_bar_gauge, rpm, LV_ANIM_OFF);
    }
}

static int64_t extract_bits(const uint8_t *data, uint8_t bit_offset, uint8_t bit_length, endian_t endian, bool is_signed) {
    uint64_t value = 0;

    if (endian == BIG_ENDIAN_ORDER) {
        for (uint8_t i = 0; i < bit_length; i++) {
            uint8_t current_bit = (data[(bit_offset + i) / 8] >> (7 - ((bit_offset + i) % 8))) & 0x01;
            value = (value << 1) | current_bit;
        }
    } else { // LITTLE_ENDIAN_ORDER
        for (uint8_t i = 0; i < bit_length; i++) {
            uint8_t current_bit = (data[(bit_offset + i) / 8] >> ((bit_offset + i) % 8)) & 0x01;
            value |= ((uint64_t)current_bit) << i;
        }
    }

    // Handle signed values
    if (is_signed && (value & ((uint64_t)1 << (bit_length - 1)))) {
        // If the highest bit is set (negative number)
        uint64_t mask = ((uint64_t)1 << bit_length) - 1;
        value = -(((~value) + 1) & mask);
    }

    return value;
}

void process_can_message(const twai_message_t *message) 
{
    static uint64_t last_panel_updates[8] = {0};
    static uint64_t last_bar_updates[2]    = {0};
    uint64_t current_time = esp_timer_get_time() / 1000; // ms

    uint32_t received_id = message->identifier;

for (int i = 0; i < 8; i++)
{
    // Only proceed if this message's CAN ID matches the config
    if (warning_configs[i].can_id == received_id)
    {
        uint8_t byte_idx = warning_configs[i].bit_position / 8;
        uint8_t bit_idx  = warning_configs[i].bit_position % 8;

        // Safety check: do we have that many bytes in the message?
        if (byte_idx < message->data_length_code)
        {
            // Extract the raw bit, invert if active_high == false
            bool bit_state = ((message->data[byte_idx] >> bit_idx) & 0x01);
            bool signal_active = bit_state;

            // If signal is on, remember time for momentary mode timeouts
            if (signal_active) {
                last_signal_times[i] = current_time;
            }

            // MOMENTARY MODE
            if (warning_configs[i].is_momentary)
            {
                // If we haven't seen the signal for >250ms, turn off
                if (current_time - last_signal_times[i] > 250)
                {
                    warning_configs[i].current_state = false;
                }
                else
                {
                    warning_configs[i].current_state = signal_active;
                }
            }
            // TOGGLE MODE (with 30ms confirm)
            else
            {
                // If we are not already waiting to confirm, and we see the bit active...
                if (!toggle_debounce[i] && signal_active)
                {
                    // Start the 30ms confirmation
                    toggle_debounce[i] = true;
                    toggle_start_time[i] = current_time;
                }
                // If we are waiting to confirm but the bit went inactive, abort
                else if (toggle_debounce[i] && !signal_active)
                {
                    toggle_debounce[i] = false; // canceled
                }
                // If we are waiting,  the bit is still active, and 30ms passed => toggle!
                else if (toggle_debounce[i] && signal_active
                         && (current_time - toggle_start_time[i] >= 30))
                {
                    warning_configs[i].current_state = !warning_configs[i].current_state;
                    toggle_debounce[i] = false; // done toggling
                }
            }

            // Update visual circle
            if (warning_circles[i])
            {
                lv_obj_set_style_bg_color(
                    warning_circles[i],
                    warning_configs[i].current_state ? 
                        warning_configs[i].active_color : 
                        lv_color_hex(0x292C29),
                    LV_PART_MAIN | LV_STATE_DEFAULT
                );
            }

            // Update label visibility
            if (warning_labels[i])
            {
                if (warning_configs[i].current_state)
                {
                    lv_obj_clear_flag(warning_labels[i], LV_OBJ_FLAG_HIDDEN);
                }
                else
                {
                    lv_obj_add_flag(warning_labels[i], LV_OBJ_FLAG_HIDDEN);
                }
            }

            // Debug
            printf("Warning %d: raw_bit=%d, sig_active=%d, mode=%s, state=%d\n",
                i+1,
                bit_state,
                signal_active,
                warning_configs[i].is_momentary ? "Momentary" : "Toggle",
                warning_configs[i].current_state);
        }
    }
}
    // Create a timer (if not already created) to handle momentary timeouts
    static lv_timer_t * warning_timer = NULL;
    if (!warning_timer) {
        warning_timer = lv_timer_create(check_warning_timeouts, 50, NULL);
    }

    // Process all other values
    for (int i = 0; i < 13; i++) {
        if (values_config[i].enabled && values_config[i].can_id == received_id) {
            uint8_t value_id = i + 1;

            uint64_t raw_value = extract_bits(
                message->data,
                values_config[i].bit_start,
                values_config[i].bit_length,
                values_config[i].endianess,
                values_config[i].is_signed
            );
            double final_value = (double)raw_value * values_config[i].scale + values_config[i].value_offset;

            // Handle panels (1-8)
            if (i < 8) {
                if (current_time - last_panel_updates[i] >= 25) {
                    snprintf(value_str_buffer, sizeof(value_str_buffer),
                        values_config[i].decimals == 0 ? "%d" : "%.*f",
                        values_config[i].decimals == 0 ? (int)final_value : values_config[i].decimals,
                        final_value);

                    if (strcmp(value_str_buffer, previous_values[i]) != 0) {
                        strcpy(previous_values[i], value_str_buffer);
                        if (ui_Value[i]) {
                            lv_label_set_text(ui_Value[i], value_str_buffer);
                        }

                        if (ui_Box[i]) {
                            if (values_config[i].warning_high_enabled &&
                                final_value > values_config[i].warning_high_threshold) {
                                lv_obj_set_style_border_color(ui_Box[i],
                                    values_config[i].warning_high_color,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
                            } else if (values_config[i].warning_low_enabled &&
                                final_value < values_config[i].warning_low_threshold) {
                                lv_obj_set_style_border_color(ui_Box[i],
                                    values_config[i].warning_low_color,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
                            } else {
                                lv_obj_set_style_border_color(ui_Box[i],
                                    lv_color_hex(0x2e2f2e),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                        }
                    }
                    last_panel_updates[i] = current_time;
                }
                continue;
            }

            // Handle bars
            if (value_id == BAR1_VALUE_ID || value_id == BAR2_VALUE_ID) {
                int bar_index = value_id - BAR1_VALUE_ID;
                if (current_time - last_bar_updates[bar_index] >= 25) {
                    if (fabs(final_value - previous_bar_values[bar_index]) >= BAR_UPDATE_THRESHOLD) {
                        previous_bar_values[bar_index] = final_value;
                        lv_obj_t *bar_obj = (value_id == BAR1_VALUE_ID) ? ui_Bar_1 : ui_Bar_2;
                        if (bar_obj) {
                            int32_t bar_value = (int32_t)final_value;
                            bar_value = bar_value < values_config[i].bar_min ? values_config[i].bar_min :
                                      (bar_value > values_config[i].bar_max ? values_config[i].bar_max : bar_value);
                            lv_bar_set_value(bar_obj, bar_value, LV_ANIM_OFF);
                        }
                    }
                    last_bar_updates[bar_index] = current_time;
                }
                continue;
            }

            // Handle RPM
            if (value_id == RPM_VALUE_ID) {
                snprintf(value_str_buffer, sizeof(value_str_buffer), "%.0f", final_value);
                if (strcmp(value_str_buffer, previous_values[i]) != 0) {
                    strcpy(previous_values[i], value_str_buffer);
                    if (ui_RPM_Value) {
                        lv_label_set_text(ui_RPM_Value, value_str_buffer);
                    }
                    int rpm_value = (int)final_value;
                    rpm_value = rpm_value < 0 ? 0 : (rpm_value > rpm_gauge_max ? rpm_gauge_max : rpm_value);
                    set_rpm_value(rpm_value);
                }
            }
            // Handle Speed
            else if (value_id == SPEED_VALUE_ID) {
                snprintf(value_str_buffer, sizeof(value_str_buffer), "%.0f", final_value);
                if (strcmp(value_str_buffer, previous_values[i]) != 0) {
                    strcpy(previous_values[i], value_str_buffer);
                    if (ui_Speed_Value) {
                        lv_label_set_text(ui_Speed_Value, value_str_buffer);
                    }
                }
            }
            // Handle Gear
            else if (value_id == GEAR_VALUE_ID) {
                snprintf(value_str_buffer, sizeof(value_str_buffer), "%.0f", final_value);
                if (strcmp(value_str_buffer, previous_values[i]) != 0) {
                    strcpy(previous_values[i], value_str_buffer);
                    if (ui_GEAR_Value) {
                        lv_label_set_text(ui_GEAR_Value, value_str_buffer);
                    }
                }
            }
        }
    }
}
/////////////////////////////////////////////	STYLES	/////////////////////////////////////////////
// Define reusable styles 
static lv_style_t box_style;
static lv_style_t common_style;

static void apply_common_roller_styles(lv_obj_t * roller) {
    // Set text color for dall items
    lv_obj_set_style_text_color(roller, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_black(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(roller, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, 0, LV_PART_SELECTED);
    lv_obj_set_style_radius(roller, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(roller, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(roller, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(roller, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(roller, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
}

// Initialize styles
static void init_styles(void) {
    // Box Style
    lv_style_init(&box_style);
    lv_style_set_radius(&box_style, 7);
    lv_style_set_bg_color(&box_style, lv_color_hex(0x2e2f2e));
    lv_style_set_bg_opa(&box_style, 0);
    lv_style_set_clip_corner(&box_style, false);
    lv_style_set_border_color(&box_style, lv_color_hex(0x2e2f2e));
    lv_style_set_border_opa(&box_style, 255);
    lv_style_set_border_width(&box_style, 3);
    lv_style_set_outline_width(&box_style, 4);
    lv_style_set_outline_pad(&box_style, 0);
}

static void init_common_style(void) {
    lv_style_init(&common_style);
    lv_style_set_radius(&common_style, 7);
    lv_style_set_pad_all(&common_style, 8); // 7px padding on all sides
    lv_style_set_bg_color(&common_style, lv_color_hex(0xFFFFFF)); // White background
    lv_style_set_bg_opa(&common_style, LV_OPA_COVER);
    lv_style_set_border_color(&common_style, lv_color_hex(0xCCCCCC)); // Light gray border
    lv_style_set_border_width(&common_style, 1);
    lv_style_set_text_color(&common_style, lv_color_black()); // Black text
    lv_style_set_text_font(&common_style, &lv_font_montserrat_12); // Common font
}

//RPM border style
static lv_obj_t* create_panel(lv_obj_t *parent, int width, int height, int x, int y, int radius, lv_color_t bg_color, int transform_angle) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_align(panel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (transform_angle != 0) {
        lv_obj_set_style_transform_angle(panel, transform_angle, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    return panel;
}
/////////////////////////////////////////////	ITEM CREATION	/////////////////////////////////////////////

void create_rpm_bar_gauge(lv_obj_t * parent_screen) {
    ui_RPM_Base_1 = create_panel(parent_screen, 800, 6, 0, -180, 0, lv_color_hex(0x2E2F2E), 0);
    ui_RPM_Base_2 = create_panel(parent_screen, 49, 22, -41, -193, 7, lv_color_hex(0x2E2F2E), 550);
    ui_RPM_Base_3 = create_panel(parent_screen, 49, 22, 105, -181, 7, lv_color_hex(0x2E2F2E), 1250);
    ui_RPM_Base_4 = create_panel(parent_screen, 111, 44, 0, -176, 7, lv_color_hex(0x2E2F2E), 0);
	lv_color_t saved_color = values_config[RPM_VALUE_ID - 1].rpm_bar_color;
    ui_Panel9 = create_panel(parent_screen, 55, 55, -372, -211, 0, saved_color, 0);
    ui_Panel10 = create_panel(parent_screen, 55, 55, 372, -211, 0, lv_color_hex(0xF4D7D7), 0);
  
    // Create the RPM bar gauge
    rpm_bar_gauge = lv_bar_create(parent_screen);
    lv_bar_set_range(rpm_bar_gauge, 0, rpm_gauge_max);
    lv_bar_set_value(rpm_bar_gauge, 0, LV_ANIM_OFF);
    lv_obj_set_size(rpm_bar_gauge, 765, 55);
    lv_obj_align(rpm_bar_gauge, LV_ALIGN_TOP_MID, 0, 2);

    // Set styles for the RPM bar gauge
    lv_obj_set_style_radius(rpm_bar_gauge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(rpm_bar_gauge, lv_color_hex(0xF3F3F3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(rpm_bar_gauge, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(rpm_bar_gauge, lv_color_hex(0xF4D7D7), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(rpm_bar_gauge, 232, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(rpm_bar_gauge, 232, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(rpm_bar_gauge, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);

	// Use the saved color for the RPM bar indicator
    lv_obj_set_style_radius(rpm_bar_gauge, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(rpm_bar_gauge, saved_color, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(rpm_bar_gauge, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(rpm_bar_gauge, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
  
    // Create the redline indicator
    ui_Redline_Indicator_1 = lv_obj_create(parent_screen);
    lv_obj_set_size(ui_Redline_Indicator_1, 89, 6);
    lv_obj_set_pos(ui_Redline_Indicator_1, 355, -180);
    lv_obj_set_align(ui_Redline_Indicator_1, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Redline_Indicator_1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_Redline_Indicator_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Redline_Indicator_1, lv_color_hex(0xE10B0B), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Redline_Indicator_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Redline_Indicator_1, lv_color_hex(0xEB706E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Redline_Indicator_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

  
#define MAX_RPM_LINES 200  // Maximum number of ticks per set

int num_rpm_lines = 0;
lv_obj_t* rpm_labels[MAX_RPM_LINES]; // Only need labels for the first set
lv_obj_t* rpm_lines[MAX_RPM_LINES * 2]; // Two sets of lines

void update_rpm_lines(lv_obj_t *parent) {
    // Delete existing lines and labels
    for (int i = 0; i < num_rpm_lines; i++) {
        if (rpm_lines[i] != NULL) {
            lv_obj_del(rpm_lines[i]);
            rpm_lines[i] = NULL;
        }
        if (i < MAX_RPM_LINES && rpm_labels[i] != NULL) {
            lv_obj_del(rpm_labels[i]);
            rpm_labels[i] = NULL;
        }
    }
    num_rpm_lines = 0;

    // Step in increments of 500 RPM for medium and main ticks
    int increments = 500; 
    int num_lines = (rpm_gauge_max / increments) + 1; // Include 0 RPM

    // Ensure we don't exceed MAX_RPM_LINES per set
    if (num_lines > MAX_RPM_LINES) {
        num_lines = MAX_RPM_LINES;
    }

    // Get the position and size of the RPM bar
    lv_coord_t bar_x = 18;
    lv_coord_t bar_y_set1 = 2;    // First set starts at px
    lv_coord_t bar_y_set2 = 44;   // Second set starts at px

    // For each tick, calculate its position for both sets
    for (int i = 0; i < num_lines; i++) {
        // Current RPM value for the tick
        int rpm_value = i * increments;

        // Calculate the x position based on rpm_value
        lv_coord_t x_pos = bar_x + ((rpm_value * 765) / rpm_gauge_max);

        // Decide which size line/tick to draw
        //    - Every 1000 RPM: main tick (width=3, height=13)
        //    - Every 500 RPM: medium tick (width=2, height=11)
        lv_coord_t line_width;
        lv_coord_t line_height;
        bool add_label = false;  // Only label the 1000s in the first set

        if ((rpm_value % 1000) == 0) {
            // Main tick
            line_width = 3;
            line_height = 12;
            add_label = true;
        } else {
            // Medium tick (500 RPM)
            line_width = 2;
            line_height = 8;
        }

        // Create the first set of lines (top row)
        lv_obj_t *line_top = lv_obj_create(parent);
        lv_obj_set_size(line_top, line_width, line_height);
        lv_obj_set_style_radius(line_top, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(line_top, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(line_top, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(line_top, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(line_top, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(line_top, LV_OBJ_FLAG_SCROLLABLE);

        // Position the line so it's centered horizontally on x_pos
        lv_coord_t adjusted_x = x_pos - (line_width / 2);
        lv_obj_set_pos(line_top, adjusted_x, bar_y_set1);

        rpm_lines[num_rpm_lines] = line_top;

        // Add a label for 1000 RPM ticks in the first set
        if (add_label) {
            lv_obj_t* label = lv_label_create(parent);

            // Display the "thousands" place (e.g., "7" for 7000)
            char rpm_str[8];
            snprintf(rpm_str, sizeof(rpm_str), "%d", rpm_value / 1000);
            lv_label_set_text(label, rpm_str);

            // Style the label
            lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_font(label, &ui_font_fugaz_17, LV_PART_MAIN | LV_STATE_DEFAULT);

            // Position the label below the line
            lv_obj_align_to(label, line_top, LV_ALIGN_OUT_BOTTOM_MID, 0, 7);

            rpm_labels[num_rpm_lines] = label;
        }

        num_rpm_lines++;

        // Create the second set of lines (bottom row, flipped height)
        lv_obj_t* line_bottom = lv_obj_create(parent);
        lv_obj_set_size(line_bottom, line_width, line_height);
        lv_obj_set_style_radius(line_bottom, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(line_bottom, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(line_bottom, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(line_bottom, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(line_bottom, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(line_bottom, LV_OBJ_FLAG_SCROLLABLE);

        // Position the bottom line flat at the bottom with its height flipped
        lv_obj_set_pos(line_bottom, adjusted_x, bar_y_set2 + (13 - line_height));

        rpm_lines[num_rpm_lines] = line_bottom;

        num_rpm_lines++;

        // Stop if we exceed the maximum number of lines
        if (num_rpm_lines >= MAX_RPM_LINES * 2) {
            break;
        }
    }
}

// Create a transparent click zone and associate with value_id
static void create_transparent_click_zone(lv_obj_t * parent, lv_obj_t * target_label, uint8_t value_id) 
{
    lv_obj_t * click_zone = lv_obj_create(parent);
    lv_obj_set_size(click_zone, 60, 60);
    lv_obj_align_to(click_zone, target_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(click_zone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(click_zone, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(click_zone, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(click_zone, LV_OBJ_FLAG_CLICKABLE);

    // Allocate memory to store value_id and pass it to the event callback
    uint8_t *id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *id_ptr = value_id;
    lv_obj_add_event_cb(click_zone, value_long_press_event_cb, LV_EVENT_LONG_PRESSED, id_ptr);
    lv_obj_add_event_cb(click_zone, free_value_id_event_cb, LV_EVENT_DELETE, id_ptr);
}

static void create_config_controls(lv_obj_t * parent, uint8_t value_id) {
    init_common_style();
    // Base alignment: center
    lv_align_t base_align = LV_ALIGN_CENTER;
    int x_offset = 0;
    uint8_t idx = value_id - 1;
  
    //**************************************************************** MENU BOXES AND BORDERS******************************************************//
    if (value_id != RPM_VALUE_ID) { // Only show for non-RPM values
    lv_obj_t * ui_Border_1 = lv_obj_create(ui_MenuScreen);
    lv_obj_set_width(ui_Border_1, 227);
    lv_obj_set_height(ui_Border_1, 30);
    lv_obj_set_x(ui_Border_1, -110);
    lv_obj_set_y(ui_Border_1, -215);
    lv_obj_set_align(ui_Border_1, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Border_1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_Border_1, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Border_1, lv_color_hex(0x2E2F2E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Border_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Border_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	char panel_text[32];
    snprintf(panel_text, sizeof(panel_text), "PANEL %d CONFIG MENU", value_id);
    lv_obj_t * ui_Panel_Config_Menu_Text = lv_label_create(ui_MenuScreen);
    lv_obj_set_width(ui_Panel_Config_Menu_Text, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Panel_Config_Menu_Text, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Panel_Config_Menu_Text, -110);
    lv_obj_set_y(ui_Panel_Config_Menu_Text, -214);
    lv_obj_set_align(ui_Panel_Config_Menu_Text, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Panel_Config_Menu_Text, panel_text);
    lv_obj_set_style_text_color(ui_Panel_Config_Menu_Text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Panel_Config_Menu_Text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Panel_Config_Menu_Text, &ui_font_fugaz_14, LV_PART_MAIN | LV_STATE_DEFAULT);
	}
	
    lv_obj_t * ui_Border_3 = lv_obj_create(ui_MenuScreen);
    lv_obj_set_width(ui_Border_3, 777);
    lv_obj_set_height(ui_Border_3, 342);
    lv_obj_set_x(ui_Border_3, 0);
    lv_obj_set_y(ui_Border_3, 58);
    lv_obj_set_align(ui_Border_3, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Border_3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_Border_3, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Border_3, lv_color_hex(0x2E2F2E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Border_3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Border_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Border_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Border_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Border_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Border_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	
    lv_obj_t * ui_Border_6 = lv_obj_create(ui_MenuScreen);
    lv_obj_set_width(ui_Border_6, 129);
    lv_obj_set_height(ui_Border_6, 50);
    lv_obj_set_x(ui_Border_6, -324);
    lv_obj_set_y(ui_Border_6, -105);
    lv_obj_set_align(ui_Border_6, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Border_6, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_Border_6, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Border_6, lv_color_hex(0x292C29), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Border_6, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Border_6, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Border_4 = lv_obj_create(ui_MenuScreen);
    lv_obj_set_width(ui_Border_4, 255);
    lv_obj_set_height(ui_Border_4, 332);
    lv_obj_set_x(ui_Border_4, -254);
    lv_obj_set_y(ui_Border_4, 55);
    lv_obj_set_align(ui_Border_4, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Border_4, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_Border_4, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Border_4, lv_color_hex(0x181818), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Border_4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Border_4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	if (value_id != RPM_VALUE_ID) { // Only show for non-RPM values
    lv_obj_t * ui_Border_5 = lv_obj_create(ui_MenuScreen);
    lv_obj_set_width(ui_Border_5, 129);
    lv_obj_set_height(ui_Border_5, 50);
    lv_obj_set_x(ui_Border_5, -57);
    lv_obj_set_y(ui_Border_5, -105);
    lv_obj_set_align(ui_Border_5, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Border_5, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_Border_5, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Border_5, lv_color_hex(0x292C29), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Border_5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Border_5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Customisation_Text = lv_label_create(ui_MenuScreen);
    lv_obj_set_width(ui_Customisation_Text, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Customisation_Text, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Customisation_Text, -56);
    lv_obj_set_y(ui_Customisation_Text, -121);
    lv_obj_set_align(ui_Customisation_Text, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Customisation_Text, "Customisation:");
    lv_obj_set_style_text_color(ui_Customisation_Text, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(ui_Customisation_Text, 255, LV_PART_MAIN);
    }

    lv_obj_t * ui_Canbus_Config_Text = lv_label_create(ui_MenuScreen);
    lv_obj_set_width(ui_Canbus_Config_Text, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Canbus_Config_Text, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Canbus_Config_Text, -324);
    lv_obj_set_y(ui_Canbus_Config_Text, -121);
    lv_obj_set_align(ui_Canbus_Config_Text, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Canbus_Config_Text, "Canbus Config:");
    lv_obj_set_style_text_color(ui_Canbus_Config_Text, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(ui_Canbus_Config_Text, 255, LV_PART_MAIN);

    //*********************************************************************************INPUTS************************************************************************/

    // Conditionally create Label input only if not Speed or RPM
    if(value_id != RPM_VALUE_ID && value_id != SPEED_VALUE_ID) {
		
	show_preconfig_menu(ui_MenuScreen);
		
    // Label input
    lv_obj_t * label_text = lv_label_create(parent);
    lv_label_set_text(label_text, "Label:");
    lv_obj_set_style_text_color(label_text, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(label_text, base_align, -196, -175);
  
    lv_obj_t * label_input = lv_textarea_create(parent);
    lv_textarea_set_one_line(label_input, true);
    lv_obj_add_style(label_input, &common_style, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(label_input, "Enter Label");
    lv_obj_set_align(label_input, LV_ALIGN_CENTER);
    lv_obj_set_width(label_input, 165);
    lv_obj_set_pos(label_input, -82, -175);
    lv_obj_add_event_cb(label_input, keyboard_event_cb, LV_EVENT_ALL, NULL);
    g_label_input[idx] = label_input;

    uint8_t *label_id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *label_id_ptr = value_id;
    lv_obj_add_event_cb(label_input, label_input_event_cb, LV_EVENT_VALUE_CHANGED, label_id_ptr);
    lv_obj_add_event_cb(label_input, free_value_id_event_cb, LV_EVENT_DELETE, label_id_ptr);
    lv_textarea_set_text(label_input, label_texts[value_id - 1]);
    }

    // CAN ID input
    lv_obj_t * can_id_text = lv_label_create(parent);
    lv_label_set_text(can_id_text, "CAN ID:");
    lv_obj_set_width(can_id_text, 110);
    lv_obj_set_style_text_color(can_id_text, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(can_id_text, base_align, -312, -87);
    lv_obj_set_style_text_align(can_id_text, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  
    // CAN ID 0x text
    lv_obj_t * can_id_0x = lv_label_create(parent);
    lv_label_set_text(can_id_0x, "0x");
    lv_obj_set_width(can_id_0x, 110);
    lv_obj_set_style_text_color(can_id_0x, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(can_id_0x, base_align, -317, -87);
    lv_obj_set_style_text_align(can_id_0x, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
  
    lv_obj_t * can_id_input = lv_textarea_create(parent);
    lv_obj_add_style(can_id_input, &common_style, LV_PART_MAIN);
    lv_textarea_set_one_line(can_id_input, true);
    lv_textarea_set_placeholder_text(can_id_input, "Enter CAN ID");
    lv_obj_set_align(can_id_input, LV_ALIGN_CENTER);
    lv_obj_set_width(can_id_input, 120);
    lv_obj_set_pos(can_id_input, -200, -87);
    lv_obj_add_event_cb(can_id_input, keyboard_event_cb, LV_EVENT_ALL, NULL);
    g_can_id_input[idx] = can_id_input;

    uint8_t *can_id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *can_id_ptr = value_id;
    lv_obj_add_event_cb(can_id_input, can_id_input_event_cb, LV_EVENT_VALUE_CHANGED, can_id_ptr);
    lv_obj_add_event_cb(can_id_input, free_value_id_event_cb, LV_EVENT_DELETE, can_id_ptr);
    {
        char can_id_str[16];
        snprintf(can_id_str, sizeof(can_id_str), "%X", values_config[value_id - 1].can_id);
        lv_textarea_set_text(can_id_input, can_id_str);  // Autopopulate CAN ID
    }

    // Endianess dropdown
    lv_obj_t * endian_text = lv_label_create(parent);
    lv_label_set_text(endian_text, "Endian:");
    lv_obj_set_width(endian_text, 110);
    lv_obj_set_style_text_color(endian_text, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(endian_text, base_align, -312, -47);
    lv_obj_set_style_text_align(endian_text, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  
    lv_obj_t * endianess_dropdown = lv_dropdown_create(parent);
    lv_obj_add_style(endianess_dropdown, &common_style, LV_PART_MAIN);
    lv_dropdown_set_options(endianess_dropdown, "Big Endian\nLittle Endian");
    lv_obj_set_align(endianess_dropdown, LV_ALIGN_CENTER);
    lv_obj_set_width(endianess_dropdown, 120);
    lv_obj_set_pos(endianess_dropdown, -200, -47);
    g_endian_dropdown[idx] = endianess_dropdown;

    uint8_t *endianess_id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *endianess_id_ptr = value_id;
    lv_obj_add_event_cb(endianess_dropdown, endianess_roller_event_cb, LV_EVENT_VALUE_CHANGED, endianess_id_ptr);
    lv_obj_add_event_cb(endianess_dropdown, free_value_id_event_cb, LV_EVENT_DELETE, endianess_id_ptr);
    lv_dropdown_set_selected(endianess_dropdown, 
        values_config[value_id - 1].endianess == BIG_ENDIAN_ORDER ? 0 : 1);  // Autopopulate Endianess

    // Bit start label and dropdown
    lv_obj_t * bit_start_label = lv_label_create(parent);
    lv_label_set_text(bit_start_label, "Bit start:");
    lv_obj_set_width(bit_start_label, 110);
    lv_obj_set_style_text_color(bit_start_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(bit_start_label, base_align, -312, -7);
    lv_obj_set_style_text_align(bit_start_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * bit_start_dropdown = lv_dropdown_create(parent);
    lv_obj_add_style(bit_start_dropdown, &common_style, LV_PART_MAIN);
    lv_dropdown_set_options(bit_start_dropdown, 
        "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59\n60\n61\n62\n63");
    lv_obj_set_align(bit_start_dropdown, LV_ALIGN_CENTER);
    lv_obj_set_width(bit_start_dropdown, 120);
    lv_obj_set_pos(bit_start_dropdown, -200, -7);
    g_bit_start_dropdown[idx] = bit_start_dropdown;

    uint8_t *bit_start_id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *bit_start_id_ptr = value_id;
    lv_obj_add_event_cb(bit_start_dropdown, bit_start_roller_event_cb, LV_EVENT_VALUE_CHANGED, bit_start_id_ptr);
    lv_obj_add_event_cb(bit_start_dropdown, free_value_id_event_cb, LV_EVENT_DELETE, bit_start_id_ptr);
    lv_dropdown_set_selected(bit_start_dropdown, values_config[value_id - 1].bit_start);  // Autopopulate Bit Start

    // Bit length label and dropdown
    lv_obj_t * bit_length_label = lv_label_create(parent);
    lv_label_set_text(bit_length_label, "Bit length:");
    lv_obj_set_width(bit_length_label, 110);
    lv_obj_set_style_text_color(bit_length_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(bit_length_label, base_align, x_offset - 312, 33);
    lv_obj_set_style_text_align(bit_length_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * bit_length_dropdown = lv_dropdown_create(parent);
    lv_obj_add_style(bit_length_dropdown, &common_style, LV_PART_MAIN);
    lv_dropdown_set_options(bit_length_dropdown, 
        "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59\n60\n61\n62\n63\n64");
    lv_obj_set_align(bit_length_dropdown, LV_ALIGN_CENTER);
    lv_obj_set_width(bit_length_dropdown, 120);
    lv_obj_set_pos(bit_length_dropdown, -200, 33);
    g_bit_length_dropdown[idx] = bit_length_dropdown;

    uint8_t *bit_length_id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *bit_length_id_ptr = value_id;
    lv_obj_add_event_cb(bit_length_dropdown, bit_length_roller_event_cb, LV_EVENT_VALUE_CHANGED, bit_length_id_ptr);
    lv_obj_add_event_cb(bit_length_dropdown, free_value_id_event_cb, LV_EVENT_DELETE, bit_length_id_ptr);
    lv_dropdown_set_selected(bit_length_dropdown, values_config[value_id - 1].bit_length - 1);  // Autopopulate Bit Length

    // Scale input
    lv_obj_t * scale_label = lv_label_create(parent);
    lv_label_set_text(scale_label, "Scale:");
    lv_obj_set_width(scale_label, 110);
    lv_obj_set_style_text_color(scale_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(scale_label, base_align, -312, 73);
    lv_obj_set_style_text_align(scale_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  
    lv_obj_t * scale_input = lv_textarea_create(parent);
    lv_obj_add_style(scale_input, &common_style, LV_PART_MAIN);
    lv_textarea_set_one_line(scale_input, true);
    lv_textarea_set_placeholder_text(scale_input, "Enter Scale");
    lv_obj_set_align(scale_input, LV_ALIGN_CENTER);
    lv_obj_set_width(scale_input, 120);
    lv_obj_set_pos(scale_input, -200, 73);
    lv_obj_add_event_cb(scale_input, keyboard_event_cb, LV_EVENT_ALL, NULL);
    g_scale_input[idx] = scale_input;

    uint8_t *scale_id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *scale_id_ptr = value_id;
    lv_obj_add_event_cb(scale_input, scale_input_event_cb, LV_EVENT_VALUE_CHANGED, scale_id_ptr);
    lv_obj_add_event_cb(scale_input, free_value_id_event_cb, LV_EVENT_DELETE, scale_id_ptr);
  
	{
    char scale_str[16];
    snprintf(scale_str, sizeof(scale_str), "%.6g", values_config[value_id - 1].scale);
    lv_textarea_set_text(scale_input, scale_str);  // Autopopulate Scale
	}

    // Value offset input
    lv_obj_t * value_offset_label = lv_label_create(parent);
    lv_label_set_text(value_offset_label, "value Offset:");
    lv_obj_set_width(value_offset_label, 110);
    lv_obj_set_style_text_color(value_offset_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(value_offset_label, base_align, -312, 113);
    lv_obj_set_style_text_align(value_offset_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  
    lv_obj_t * value_offset_input = lv_textarea_create(parent);
    lv_obj_add_style(value_offset_input, &common_style, LV_PART_MAIN);
    lv_textarea_set_one_line(value_offset_input, true);
    lv_textarea_set_placeholder_text(value_offset_input, "Enter Value Offset");
    lv_obj_set_align(value_offset_input, LV_ALIGN_CENTER);
    lv_obj_set_width(value_offset_input, 120);
    lv_obj_set_pos(value_offset_input, -200, 113);
    lv_obj_add_event_cb(value_offset_input, keyboard_event_cb, LV_EVENT_ALL, NULL);
    g_offset_input[idx] = value_offset_input;

    uint8_t *value_offset_id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *value_offset_id_ptr = value_id;
    lv_obj_add_event_cb(value_offset_input, value_offset_input_event_cb, LV_EVENT_VALUE_CHANGED, value_offset_id_ptr);
    lv_obj_add_event_cb(value_offset_input, free_value_id_event_cb, LV_EVENT_DELETE, value_offset_id_ptr);
  
	{
    char voffset_str[16];
    snprintf(voffset_str, sizeof(voffset_str), "%.6g", values_config[value_id - 1].value_offset);
    lv_textarea_set_text(value_offset_input, voffset_str);  // Autopopulate Value Offset
	}

    // Decimal label and dropdown
    lv_obj_t * decimal_label = lv_label_create(parent);
    lv_label_set_text(decimal_label, "Decimals:");
    lv_obj_set_width(decimal_label, 110);
    lv_obj_set_style_text_color(decimal_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(decimal_label, base_align,-312,153);
    lv_obj_set_style_text_align(decimal_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * decimal_dropdown = lv_dropdown_create(parent);
    lv_obj_add_style(decimal_dropdown, &common_style, LV_PART_MAIN);
    lv_dropdown_set_options(decimal_dropdown, "0\n1\n2");
    lv_obj_set_align(decimal_dropdown, LV_ALIGN_CENTER);
    lv_obj_set_width(decimal_dropdown, 120);
    lv_obj_set_pos(decimal_dropdown, -200, 153);
    g_decimals_dropdown[idx] = decimal_dropdown;

	uint8_t *decimal_id_ptr = lv_mem_alloc(sizeof(uint8_t));
	*decimal_id_ptr = value_id;
	lv_obj_add_event_cb(decimal_dropdown, decimal_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, decimal_id_ptr);
	lv_obj_add_event_cb(decimal_dropdown, free_value_id_event_cb, LV_EVENT_DELETE, decimal_id_ptr);
	lv_dropdown_set_selected(decimal_dropdown, values_config[value_id - 1].decimals);
	
	// Type label and dropdown
    lv_obj_t * type_label = lv_label_create(parent);
    lv_label_set_text(type_label, "Type:");
    lv_obj_set_width(type_label, 110);
    lv_obj_set_style_text_color(type_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(type_label, base_align, -312, 193);
    lv_obj_set_style_text_align(type_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * type_dropdown = lv_dropdown_create(parent);
    lv_obj_add_style(type_dropdown, &common_style, LV_PART_MAIN);
    lv_dropdown_set_options(type_dropdown, "Unsigned\nSigned");
    lv_obj_set_align(type_dropdown, LV_ALIGN_CENTER);
    lv_obj_set_width(type_dropdown, 120);
    lv_obj_set_pos(type_dropdown, -200, 193);
    g_type_dropdown[idx] = type_dropdown;

    uint8_t *type_id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *type_id_ptr = value_id;
    lv_obj_add_event_cb(type_dropdown, type_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, type_id_ptr);
    lv_obj_add_event_cb(type_dropdown, free_value_id_event_cb, LV_EVENT_DELETE, type_id_ptr);
    lv_dropdown_set_selected(type_dropdown, values_config[value_id - 1].is_signed ? 1 : 0);
  
///////////////////////////////////	CUSTOMISATIONS///////////////////////////////////////////////////////////////// 
}
// Load menu screen
static void load_menu_screen_for_value(uint8_t value_id) {
    ui_MenuScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_MenuScreen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_MenuScreen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui_MenuScreen, LV_OBJ_FLAG_SCROLLABLE);

    // Create the keyboard
    keyboard = lv_keyboard_create(ui_MenuScreen);
    lv_obj_set_parent(keyboard, lv_layer_top());
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(keyboard, keyboard_ready_event_cb, LV_EVENT_READY, NULL);

    // Create a close button at the bottom center as default
    lv_obj_t * close_btn = lv_btn_create(ui_MenuScreen);
    lv_obj_t * close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Save");
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x202829), LV_PART_MAIN);
    lv_obj_set_align(close_btn, LV_ALIGN_CENTER);
    lv_obj_set_pos(close_btn, 250, 200);
    lv_obj_center(close_label);

    if (value_id >= 1 && value_id <= 8) {
        create_menu_objects(ui_MenuScreen, value_id);
      
     // High Warning Threshold
    lv_obj_t * warning_high_label = lv_label_create(ui_MenuScreen);
    lv_label_set_text(warning_high_label, "High Warning:");
    lv_obj_set_style_text_color(warning_high_label, lv_color_hex(0xCCCC), 0);
    lv_obj_align(warning_high_label, LV_ALIGN_CENTER, 220, -87);

    lv_obj_t * warning_high_input = lv_textarea_create(ui_MenuScreen);
    lv_obj_add_style(warning_high_input, &common_style, LV_PART_MAIN);
    lv_textarea_set_one_line(warning_high_input, true);
    lv_obj_set_width(warning_high_input, 100);
    lv_obj_align(warning_high_input, LV_ALIGN_CENTER, 320, -87);
    lv_obj_add_event_cb(warning_high_input, keyboard_event_cb, LV_EVENT_ALL, NULL);

    // High Warning Color
    lv_obj_t * warning_high_color_label = lv_label_create(ui_MenuScreen);
    lv_label_set_text(warning_high_color_label, "High Color:");
    lv_obj_set_style_text_color(warning_high_color_label, lv_color_hex(0xCCCC), 0);
    lv_obj_align(warning_high_color_label, LV_ALIGN_CENTER, 220, -47);

    lv_obj_t * warning_high_color_dd = lv_dropdown_create(ui_MenuScreen);
    lv_dropdown_set_options(warning_high_color_dd, "Red\nBlue");
    lv_obj_set_width(warning_high_color_dd, 100);
    lv_obj_align(warning_high_color_dd, LV_ALIGN_CENTER, 320, -47);
    lv_obj_add_style(warning_high_color_dd, &common_style, LV_PART_MAIN);

    // Low Warning Threshold
    lv_obj_t * warning_low_label = lv_label_create(ui_MenuScreen);
    lv_label_set_text(warning_low_label, "Low Warning:");
    lv_obj_set_style_text_color(warning_low_label, lv_color_hex(0xCCCC), 0);
    lv_obj_align(warning_low_label, LV_ALIGN_CENTER, 220, -7);

    lv_obj_t * warning_low_input = lv_textarea_create(ui_MenuScreen);
    lv_obj_add_style(warning_low_input, &common_style, LV_PART_MAIN);
    lv_textarea_set_one_line(warning_low_input, true);
    lv_obj_set_width(warning_low_input, 100);
    lv_obj_align(warning_low_input, LV_ALIGN_CENTER, 320, -7);
    lv_obj_add_event_cb(warning_low_input, keyboard_event_cb, LV_EVENT_ALL, NULL);

    // Low Warning Color
    lv_obj_t * warning_low_color_label = lv_label_create(ui_MenuScreen);
    lv_label_set_text(warning_low_color_label, "Low Color:");
    lv_obj_set_style_text_color(warning_low_color_label, lv_color_hex(0xCCCC), 0);
    lv_obj_align(warning_low_color_label, LV_ALIGN_CENTER, 220, 33);

    lv_obj_t * warning_low_color_dd = lv_dropdown_create(ui_MenuScreen);
    lv_dropdown_set_options(warning_low_color_dd, "Red\nBlue");
    lv_obj_set_width(warning_low_color_dd, 100);
    lv_obj_align(warning_low_color_dd, LV_ALIGN_CENTER, 320, 33);
    lv_obj_add_style(warning_low_color_dd, &common_style, LV_PART_MAIN);

    // Set current values
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", values_config[value_id - 1].warning_high_threshold);
    lv_textarea_set_text(warning_high_input, buf);
    snprintf(buf, sizeof(buf), "%.2f", values_config[value_id - 1].warning_low_threshold);
    lv_textarea_set_text(warning_low_input, buf);

    // Set current colors
    bool is_high_red = values_config[value_id - 1].warning_high_color.full == lv_color_hex(0xFF0000).full;
    bool is_low_red = values_config[value_id - 1].warning_low_color.full == lv_color_hex(0xFF0000).full;
    lv_dropdown_set_selected(warning_high_color_dd, is_high_red ? 0 : 1);
    lv_dropdown_set_selected(warning_low_color_dd, is_low_red ? 0 : 1);

    // Add event handlers
    uint8_t *id_ptr = lv_mem_alloc(sizeof(uint8_t));
    *id_ptr = value_id;
  
    lv_obj_add_event_cb(warning_high_input, warning_high_threshold_event_cb, LV_EVENT_VALUE_CHANGED, id_ptr);
    lv_obj_add_event_cb(warning_low_input, warning_low_threshold_event_cb, LV_EVENT_VALUE_CHANGED, id_ptr);
    lv_obj_add_event_cb(warning_high_color_dd, warning_high_color_event_cb, LV_EVENT_VALUE_CHANGED, id_ptr);
    lv_obj_add_event_cb(warning_low_color_dd, warning_low_color_event_cb, LV_EVENT_VALUE_CHANGED, id_ptr);

    } else if (value_id == RPM_VALUE_ID) {
        // RPM value
        const char *current_rpm_value = lv_label_get_text(ui_RPM_Value);
        create_config_controls(ui_MenuScreen, RPM_VALUE_ID);
        create_rpm_bar_gauge(ui_MenuScreen);
		update_rpm_lines(ui_MenuScreen);

        // Create RPM label
        lv_obj_t * rpm_label = lv_label_create(ui_MenuScreen);
        lv_label_set_text(rpm_label, "RPM");
        // Apply same styling as on Screen3 for RPM label
        lv_obj_set_style_text_color(rpm_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(rpm_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(rpm_label, &ui_font_fugaz_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align_to(rpm_label, ui_RPM_Label, LV_ALIGN_CENTER, 0, 0);

        // Create RPM value
        lv_obj_t * rpm_value = lv_label_create(ui_MenuScreen);
        lv_label_set_text(rpm_value, current_rpm_value);
        // Apply same styling as on Screen3 for RPM value
        lv_obj_set_style_text_color(rpm_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(rpm_value, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(rpm_value, &ui_font_fugaz_28, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align_to(rpm_value, ui_RPM_Value, LV_ALIGN_CENTER, 0, 0);
      
    	lv_obj_t * max_rpm_text = lv_label_create(ui_MenuScreen);
    	lv_label_set_text(max_rpm_text, "RPM Gauge:");
    	lv_obj_set_style_text_color(max_rpm_text, lv_color_hex(0xCCCCCC), 0);
    	lv_obj_align(max_rpm_text, LV_ALIGN_CENTER, 220, -87);
   		lv_obj_set_style_text_align(max_rpm_text, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    	
		// Add RPM Gauge Roller
		lv_obj_t * rpm_gauge_roller = lv_roller_create(ui_MenuScreen);
		lv_roller_set_options(rpm_gauge_roller,
   		 "3000\n4000\n5000\n6000\n7000\n8000\n9000\n10000\n11000\n12000",
   		 LV_ROLLER_MODE_NORMAL);
		uint16_t selected_index = 0;
		if (rpm_gauge_max >= 3000 && rpm_gauge_max <= 12000) {
		    selected_index = (rpm_gauge_max - 3000) / 1000;
		}
		lv_roller_set_selected(rpm_gauge_roller, selected_index, LV_ANIM_OFF);
		lv_roller_set_visible_row_count(rpm_gauge_roller, 1);
		lv_obj_set_width(rpm_gauge_roller, 80);
		lv_obj_set_height(rpm_gauge_roller, 35);
		lv_obj_align(rpm_gauge_roller, LV_ALIGN_CENTER, 320, -87); // Position in menu
		lv_obj_add_event_cb(rpm_gauge_roller, rpm_gauge_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		apply_common_roller_styles(rpm_gauge_roller);
		
		lv_obj_t * rpm_ecu_dropdown_label = lv_label_create(ui_MenuScreen);
    	lv_label_set_text(rpm_ecu_dropdown_label, "ECU Presets:");
    	lv_obj_set_style_text_color(rpm_ecu_dropdown_label, lv_color_hex(0xCCCCCC), 0);
    	lv_obj_align(rpm_ecu_dropdown_label, LV_ALIGN_CENTER, -50, -87);
    	
    	lv_obj_t * rpm_ecu_dropdown = lv_dropdown_create(ui_MenuScreen);
    	// Provide the three options
    	lv_dropdown_set_options(rpm_ecu_dropdown, "Custom\nMaxxECU\nHaltech");
    	lv_obj_align(rpm_ecu_dropdown, LV_ALIGN_CENTER, 80, -87);
    	lv_obj_set_width(rpm_ecu_dropdown, 120);
    	lv_obj_add_style(rpm_ecu_dropdown, &common_style, LV_PART_MAIN);

    	// Attach an event callback so we can detect selection changes
    	lv_obj_add_event_cb(rpm_ecu_dropdown, rpm_ecu_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    	
    	// RPM Color dropdown
		lv_obj_t * rpm_color_label = lv_label_create(ui_MenuScreen);
		lv_label_set_text(rpm_color_label, "RPM Color:");
		lv_obj_set_style_text_color(rpm_color_label, lv_color_hex(0xCCCCCC), 0);
		lv_obj_align(rpm_color_label, LV_ALIGN_CENTER, -50, -47); // 40px below ECU presets (-87 + 40)
		
		lv_obj_t * rpm_color_dropdown = lv_dropdown_create(ui_MenuScreen);
		lv_obj_add_style(rpm_color_dropdown, &common_style, LV_PART_MAIN);
		lv_dropdown_set_options(rpm_color_dropdown, 
		    "Green\n"
		    "Light Blue\n"
		    "Yellow\n"
		    "Orange\n"
		    "Red\n"
		    "Dark Blue\n"
		    "Purple\n"
		    "Magenta\n"
		    "Pink");
		lv_obj_set_width(rpm_color_dropdown, 120); // Same width as ECU presets
		lv_obj_align(rpm_color_dropdown, LV_ALIGN_CENTER, 80, -47);

		// Add event handler
		lv_obj_add_event_cb(rpm_color_dropdown, rpm_color_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		uint16_t saved_color = values_config[RPM_VALUE_ID - 1].rpm_bar_color.full;

if (saved_color == lv_color_hex(0x00E1FF).full) selected_index = 1;
else if (saved_color == lv_color_hex(0xFFEB3B).full) selected_index = 2;
else if (saved_color == lv_color_hex(0xFF9800).full) selected_index = 3;
else if (saved_color == lv_color_hex(0xFF0000).full) selected_index = 4;
else if (saved_color == lv_color_hex(0x0000FF).full) selected_index = 5;
else if (saved_color == lv_color_hex(0x9C27B0).full) selected_index = 6;
else if (saved_color == lv_color_hex(0xFF00FF).full) selected_index = 7;
else if (saved_color == lv_color_hex(0xFF4081).full) selected_index = 8;
lv_dropdown_set_selected(rpm_color_dropdown, selected_index);

    } else if (value_id == SPEED_VALUE_ID) {
        // Speed value
        const char *current_speed_value = lv_label_get_text(ui_Speed_Value);
        const char *current_kmh_label = lv_label_get_text(ui_Kmh);
		create_config_controls(ui_MenuScreen, SPEED_VALUE_ID);

        // Create Speed value label
        lv_obj_t * speed_value = lv_label_create(ui_MenuScreen);
        lv_label_set_text(speed_value, current_speed_value);
        // Apply same styling as on Screen3 for Speed value
        lv_obj_set_style_text_color(speed_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(speed_value, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(speed_value, &ui_font_fugaz_56, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_align(speed_value, LV_ALIGN_CENTER, -328, -185);

        // Create Kmh label
        lv_obj_t * kmh_label = lv_label_create(ui_MenuScreen);
        lv_label_set_text(kmh_label, current_kmh_label);
        // Apply same styling as on Screen3 for Kmh label
        lv_obj_set_style_text_color(kmh_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(kmh_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(kmh_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_align(kmh_label, LV_ALIGN_CENTER, -295, -156);
    	
    } else if (value_id == GEAR_VALUE_ID) {
		create_config_controls(ui_MenuScreen, GEAR_VALUE_ID);
	
		ui_Gear_Label = lv_label_create(ui_MenuScreen);
    	lv_label_set_text(ui_Gear_Label, label_texts[GEAR_VALUE_ID - 1]); 
    	lv_obj_set_style_text_color(ui_Gear_Label, lv_color_hex(0xFFFFFF), 0);
    	lv_obj_align(ui_Gear_Label, LV_ALIGN_CENTER, -312, -216); 
	
	} else if (value_id == BAR1_VALUE_ID || value_id == BAR2_VALUE_ID) {
    	create_config_controls(ui_MenuScreen, value_id);
  
    	// Create bar label
    	lv_obj_t * bar_label = lv_label_create(ui_MenuScreen);
    	lv_label_set_text(bar_label, label_texts[value_id - 1]);
    	lv_obj_set_style_text_font(bar_label, &ui_font_fugaz_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_text_color(bar_label, lv_color_hex(0xFFFFFF), 0);
    	lv_obj_align(bar_label, LV_ALIGN_CENTER, -312, -210);
  
    	// Create bar visualization
    	lv_obj_t * menu_bar = lv_bar_create(ui_MenuScreen);
    	lv_obj_set_size(menu_bar, 140, 25);
    	lv_obj_align(menu_bar, LV_ALIGN_CENTER, -312, -179);
    	lv_obj_set_style_bg_color(menu_bar, lv_color_hex(0x2e2f2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_bg_opa(menu_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_radius(menu_bar, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_bg_color(menu_bar, lv_color_hex(0x2e2f2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_bg_opa(menu_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_border_color(menu_bar, lv_color_hex(0x2e2f2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_border_opa(menu_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_border_width(menu_bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_outline_width(menu_bar, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_outline_pad(menu_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_pad_left(menu_bar, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_pad_right(menu_bar, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_pad_top(menu_bar, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    	lv_obj_set_style_pad_bottom(menu_bar, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    	
    	lv_obj_set_style_radius(menu_bar, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    	lv_obj_set_style_bg_color(menu_bar, lv_color_hex(0x38FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    	lv_obj_set_style_bg_opa(menu_bar, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    	
    	// Bar minimum input
    	lv_obj_t * bar_min_label = lv_label_create(ui_MenuScreen);
    	lv_label_set_text(bar_min_label, "Bar Min Value:");
    	lv_obj_set_style_text_color(bar_min_label, lv_color_hex(0xFFFFFF), 0);
    	lv_obj_align(bar_min_label, LV_ALIGN_CENTER, -50, -87);
  
    	lv_obj_t * bar_min_input = lv_textarea_create(ui_MenuScreen);
    	lv_obj_add_style(bar_min_input, &common_style, LV_PART_MAIN);
    	lv_textarea_set_one_line(bar_min_input, true);
    	lv_obj_set_width(bar_min_input, 100);
    	lv_obj_align(bar_min_input, LV_ALIGN_CENTER, 80, -87);
    	lv_obj_add_event_cb(bar_min_input, keyboard_event_cb, LV_EVENT_ALL, NULL);
    	lv_obj_set_user_data(bar_min_input, (void*)1); // Mark as min input
  
    	// Bar maximum input
    	lv_obj_t * bar_max_label = lv_label_create(ui_MenuScreen);
    	lv_label_set_text(bar_max_label, "Bar Max Value:");
    	lv_obj_set_style_text_color(bar_max_label, lv_color_hex(0xFFFFFF), 0);
    	lv_obj_align(bar_max_label, LV_ALIGN_CENTER, -50, -47);
  
   		lv_obj_t * bar_max_input = lv_textarea_create(ui_MenuScreen);
    	lv_obj_add_style(bar_max_input, &common_style, LV_PART_MAIN);
    	lv_textarea_set_one_line(bar_max_input, true);
    	lv_obj_set_width(bar_max_input, 100);
    	lv_obj_align(bar_max_input, LV_ALIGN_CENTER, 80, -47);
    	lv_obj_add_event_cb(bar_max_input, keyboard_event_cb, LV_EVENT_ALL, NULL);
  
    	// Add event callbacks
    	uint8_t *id_ptr = lv_mem_alloc(sizeof(uint8_t));
    	*id_ptr = value_id;
    	lv_obj_add_event_cb(bar_min_input, bar_range_input_event_cb, LV_EVENT_VALUE_CHANGED, id_ptr);
    	lv_obj_add_event_cb(bar_max_input, bar_range_input_event_cb, LV_EVENT_VALUE_CHANGED, id_ptr);
  
    	// Set current values
    	char buf[16];
    	snprintf(buf, sizeof(buf), "%d", values_config[value_id - 1].bar_min);
    	lv_textarea_set_text(bar_min_input, buf);
    	snprintf(buf, sizeof(buf), "%d", values_config[value_id - 1].bar_max);
    	lv_textarea_set_text(bar_max_input, buf);
}	
	
	lv_obj_move_foreground(close_btn);
	
    // When close is pressed, go back to ui_Screen3
    lv_obj_add_event_cb(close_btn, close_menu_event_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(ui_MenuScreen);
}

void create_warning_config_menu(uint8_t warning_idx) {
    init_common_style();
    
    // Allocate memory for input objects array - increase size to 7 to include the toggle dropdown
    lv_obj_t** input_objects = lv_mem_alloc(7 * sizeof(lv_obj_t*));
    if (!input_objects) {
        printf("Failed to allocate memory for input objects\n");
        return;
    }
    // Initialize all pointers to NULL
    for(int i = 0; i < 7; i++) {
        input_objects[i] = NULL;
    }

    // Allocate memory for preview objects
    lv_obj_t** preview_objects = lv_mem_alloc(2 * sizeof(lv_obj_t*));
    if (!preview_objects) {
        lv_mem_free(input_objects);
        printf("Failed to allocate memory for preview objects\n");
        return;
    }
    preview_objects[0] = NULL;
    preview_objects[1] = NULL;

    // Create the configuration screen
    lv_obj_t* config_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(config_screen, lv_color_hex(0x0000), 0);
    lv_obj_set_style_bg_opa(config_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(config_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Create save data structure
    warning_save_data_t* save_data = lv_mem_alloc(sizeof(warning_save_data_t));
    if (!save_data) {
        lv_mem_free(input_objects);
        lv_mem_free(preview_objects);
        printf("Failed to allocate memory for save data\n");
        return;
    }
    save_data->warning_idx = warning_idx;
    save_data->input_objects = input_objects;
    save_data->preview_objects = preview_objects;
  
    // Create main border/background panel
	lv_obj_t* main_border = lv_obj_create(config_screen);
	lv_obj_set_width(main_border, 780); 
	lv_obj_set_height(main_border, 325);
	lv_obj_set_align(main_border, LV_ALIGN_CENTER);
	lv_obj_set_y(main_border, 67);  // Vertical
	lv_obj_clear_flag(main_border, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_radius(main_border, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(main_border, lv_color_hex(0x292C29), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(main_border, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(main_border, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	
	lv_obj_t * input_border = lv_obj_create(config_screen);
    lv_obj_set_width(input_border, 275);
    lv_obj_set_height(input_border, 310);
    lv_obj_set_x(input_border, -244);
    lv_obj_set_y(input_border, 67);
    lv_obj_set_align(input_border, LV_ALIGN_CENTER);
    lv_obj_clear_flag(input_border, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(input_border, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(input_border, lv_color_hex(0x181818), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(input_border, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(input_border, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  
    // Create preview warning circle in exact Screen3 position
    lv_obj_t* preview_circle = lv_obj_create(config_screen);
    lv_obj_set_width(preview_circle, 15);
    lv_obj_set_height(preview_circle, 15);
    lv_obj_set_x(preview_circle, warning_positions[warning_idx].x);
    lv_obj_set_y(preview_circle, warning_positions[warning_idx].y);
    lv_obj_set_align(preview_circle, LV_ALIGN_CENTER);
    lv_obj_clear_flag(preview_circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(preview_circle, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(preview_circle, warning_configs[warning_idx].active_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(preview_circle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(preview_circle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Create preview warning label in exact Screen3 position
    lv_obj_t* preview_label = lv_label_create(config_screen);
    lv_obj_set_width(preview_label, 51);
    lv_obj_set_height(preview_label, 41);
    lv_obj_set_x(preview_label, warning_positions[warning_idx].x);
    lv_obj_set_y(preview_label, -112); // Same y-position as in Screen3
    lv_obj_set_align(preview_label, LV_ALIGN_CENTER);
    lv_label_set_text(preview_label, warning_configs[warning_idx].label);
    lv_obj_set_style_text_color(preview_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(preview_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(preview_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(preview_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    preview_objects[1] = preview_label;
  
    // Create the keyboard
    keyboard = lv_keyboard_create(config_screen);
    lv_obj_set_parent(keyboard, lv_layer_top());
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(keyboard, keyboard_ready_event_cb, LV_EVENT_READY, NULL);

    // Create title
    lv_obj_t* title = lv_label_create(config_screen);
    lv_label_set_text_fmt(title, "Warning %d Configuration", warning_idx + 1);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

	// Create a container for inputs
	lv_obj_t* inputs_container = lv_obj_create(config_screen);
	lv_obj_set_size(inputs_container, 800, 480);  // Adjusted size to fit within the border
	lv_obj_align(inputs_container, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_bg_opa(inputs_container, 0, 0);
	lv_obj_set_style_border_opa(inputs_container, 0, 0);
	lv_obj_clear_flag(inputs_container, LV_OBJ_FLAG_SCROLLABLE);

// Warning label input (moved to top)
lv_obj_t* label_text_label = lv_label_create(inputs_container);
lv_label_set_text(label_text_label, "Warning Label:");
lv_obj_set_width(label_text_label, 110);
lv_obj_set_style_text_align(label_text_label, LV_TEXT_ALIGN_LEFT, 0);
lv_obj_align(label_text_label, LV_ALIGN_CENTER, -312, -47);  // Was 73, now -47
lv_obj_set_style_text_color(label_text_label, lv_color_hex(0xCCCCCC), 0);

input_objects[3] = lv_textarea_create(inputs_container);
lv_obj_add_style(input_objects[3], &common_style, LV_PART_MAIN);
lv_textarea_set_one_line(input_objects[3], true);
lv_obj_set_width(input_objects[3], 120);
lv_obj_align(input_objects[3], LV_ALIGN_CENTER, -180, -47);  // Was 73, now -47
lv_obj_add_event_cb(input_objects[3], keyboard_event_cb, LV_EVENT_ALL, NULL);
lv_textarea_set_text(input_objects[3], warning_configs[warning_idx].label);

// CAN ID input (moved down)
lv_obj_t* can_id_label = lv_label_create(inputs_container);
lv_label_set_text(can_id_label, "CAN ID (hex):");
lv_obj_set_width(can_id_label, 110);
lv_obj_set_style_text_align(can_id_label, LV_TEXT_ALIGN_LEFT, 0);
lv_obj_align(can_id_label, LV_ALIGN_CENTER, -312, -7);  // Was -47, now -7
lv_obj_set_style_text_color(can_id_label, lv_color_hex(0xCCCCCC), 0);

input_objects[0] = lv_textarea_create(inputs_container);
lv_obj_add_style(input_objects[0], &common_style, LV_PART_MAIN);
lv_textarea_set_one_line(input_objects[0], true);
lv_obj_set_width(input_objects[0], 120);
lv_obj_align(input_objects[0], LV_ALIGN_CENTER, -180, -7);  // Was -47, now -7
lv_obj_add_event_cb(input_objects[0], keyboard_event_cb, LV_EVENT_ALL, NULL);
char can_id_text[32];
snprintf(can_id_text, sizeof(can_id_text), "%X", warning_configs[warning_idx].can_id);
lv_textarea_set_text(input_objects[0], can_id_text);

// Bit position dropdown
lv_obj_t* bit_pos_label = lv_label_create(inputs_container);
lv_label_set_text(bit_pos_label, "Bit Position:");
lv_obj_set_width(bit_pos_label, 110);
lv_obj_set_style_text_align(bit_pos_label, LV_TEXT_ALIGN_LEFT, 0);
lv_obj_align(bit_pos_label, LV_ALIGN_CENTER, -312, 33);  // Was -7, now 33
lv_obj_set_style_text_color(bit_pos_label, lv_color_hex(0xCCCCCC), 0);

input_objects[1] = lv_dropdown_create(inputs_container);
lv_obj_add_style(input_objects[1], &common_style, LV_PART_MAIN);
lv_dropdown_set_options(input_objects[1], "0\n1\n2\n3\n4\n5\n6\n7");
lv_obj_set_width(input_objects[1], 120);
lv_obj_align(input_objects[1], LV_ALIGN_CENTER, -180, 33);  // Was -7, now 33
lv_dropdown_set_selected(input_objects[1], warning_configs[warning_idx].bit_position);

// Highlighted color dropdown
lv_obj_t* color_label = lv_label_create(inputs_container);
lv_label_set_text(color_label, "Active Colour:");
lv_obj_set_width(color_label, 110);
lv_obj_set_style_text_align(color_label, LV_TEXT_ALIGN_LEFT, 0);
lv_obj_align(color_label, LV_ALIGN_CENTER, -312, 73);  // No change needed
lv_obj_set_style_text_color(color_label, lv_color_hex(0xCCCCCC), 0);

input_objects[4] = lv_dropdown_create(inputs_container);
lv_obj_add_style(input_objects[4], &common_style, LV_PART_MAIN);
lv_dropdown_set_options(input_objects[4], "Green\nBlue\nOrange\nRed\nYellow");
lv_obj_set_width(input_objects[4], 120);
lv_obj_align(input_objects[4], LV_ALIGN_CENTER, -180, 73);  // No change needed

// Set the current color selection based on the saved configuration
lv_color_t current_color = warning_configs[warning_idx].active_color;
uint8_t selected_color = 0; // Default to Green
if (current_color.full == lv_color_hex(0x0000FF).full) selected_color = 1; // Blue
else if (current_color.full == lv_color_hex(0xFFA500).full) selected_color = 2; // Orange
else if (current_color.full == lv_color_hex(0xFF0000).full) selected_color = 3; // Red
else if (current_color.full == lv_color_hex(0xFFFF00).full) selected_color = 4; // Yellow
lv_dropdown_set_selected(input_objects[4], selected_color);
                       
// Add Toggle Mode dropdown
lv_obj_t* toggle_mode_label = lv_label_create(inputs_container);
lv_label_set_text(toggle_mode_label, "Toggle Mode:");
lv_obj_set_width(toggle_mode_label, 110);
lv_obj_set_style_text_align(toggle_mode_label, LV_TEXT_ALIGN_LEFT, 0);
lv_obj_set_style_text_color(toggle_mode_label, lv_color_hex(0xCCCC), 0);
lv_obj_align(toggle_mode_label, LV_ALIGN_CENTER, -312, 113);

input_objects[5] = lv_dropdown_create(inputs_container);
lv_obj_add_style(input_objects[5], &common_style, LV_PART_MAIN);
lv_dropdown_set_options(input_objects[5], "On/Off\nMomentary");
lv_obj_set_width(input_objects[5], 120);
lv_obj_align(input_objects[5], LV_ALIGN_CENTER, -180, 113);
lv_dropdown_set_selected(input_objects[5], 
    warning_configs[warning_idx].is_momentary ? 1 : 0);

    // Save button
    lv_obj_t* save_btn = lv_btn_create(config_screen);
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
  
    lv_obj_add_event_cb(save_btn, save_warning_config_cb, LV_EVENT_CLICKED, save_data);

    lv_scr_load(config_screen);
}

static void create_menu_objects(lv_obj_t * parent, uint8_t value_id) {
    uint8_t idx = value_id - 1; // Adjust index (value_id is 1 to 8, arrays are 0-based)

    // Label
    ui_Label[idx] = lv_label_create(parent);
    lv_label_set_text(ui_Label[idx], label_texts[idx]);
    lv_obj_set_style_text_color(ui_Label[idx], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ui_Label[idx], &ui_font_fugaz_14, 0);
    lv_obj_set_x(ui_Label[idx], -312);
    lv_obj_set_y(ui_Label[idx], -216);
    lv_obj_set_align(ui_Label[idx], LV_ALIGN_CENTER);

    // Value
    ui_Value[idx] = lv_label_create(parent);
    lv_label_set_text(ui_Value[idx], "0");
    lv_obj_set_style_text_color(ui_Value[idx], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ui_Value[idx], &ui_font_Manrope_35_BOLD, 0);
    lv_obj_set_x(ui_Value[idx], -312);
    lv_obj_set_y(ui_Value[idx], -178);
    lv_obj_set_align(ui_Value[idx], LV_ALIGN_CENTER);

    // Create and add the corresponding box and store it in ui_Box array
    ui_Box[idx] = lv_obj_create(parent);
    lv_obj_set_size(ui_Box[idx], 155, 92);
    lv_obj_set_pos(ui_Box[idx], -312, -185);
    lv_obj_set_align(ui_Box[idx], LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Box[idx], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(ui_Box[idx], &box_style, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Add configuration controls for any value
    create_config_controls(parent, value_id);
}

// Creation and initializing Boxes and Arcs
static void init_boxes_and_arcs(void) {
    for(uint8_t i = 0; i < 8; i++) {
        // Create Box
        ui_Box[i] = lv_obj_create(ui_Screen3);
        lv_obj_set_size(ui_Box[i], 155, 92);
        lv_obj_set_pos(ui_Box[i], box_positions[i][0], box_positions[i][1]);
        lv_obj_set_align(ui_Box[i], LV_ALIGN_CENTER);
        lv_obj_clear_flag(ui_Box[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(ui_Box[i], &box_style, LV_PART_MAIN | LV_STATE_DEFAULT);

    }
}


/////////////////////////////////////////////	UI_SCREEN3__SCREEN_INIT	/////////////////////////////////////////////


void ui_Screen3_screen_init(void)
{
    init_styles();
    init_values_config_defaults();
    init_warning_configs();
    load_values_config_from_nvs();
    load_warning_configs_from_nvs();
  
    ui_Screen3 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen3, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Screen3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

	init_boxes_and_arcs();
	create_rpm_bar_gauge(ui_Screen3);
	update_rpm_lines(ui_Screen3);
	
    // Initialize labels, units, bars, and values
    for (uint8_t i = 0; i < 8; i++) {
        // Create label
        ui_Label[i] = lv_label_create(ui_Screen3);
        lv_label_set_text(ui_Label[i], label_texts[i]);
        lv_obj_set_style_text_color(ui_Label[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_Label[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_Label[i], &ui_font_fugaz_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_x(ui_Label[i], label_positions[i][0]);
        lv_obj_set_y(ui_Label[i], label_positions[i][1]);
        lv_obj_set_align(ui_Label[i], LV_ALIGN_CENTER);

        // Create value label
        ui_Value[i] = lv_label_create(ui_Screen3);
        lv_label_set_text(ui_Value[i], "0");
        lv_obj_set_style_text_color(ui_Value[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_Value[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_Value[i], &ui_font_Manrope_35_BOLD, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_Value[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(ui_Value[i], 140);
        lv_label_set_long_mode(ui_Value[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_x(ui_Value[i], value_positions[i][0]);
        lv_obj_set_y(ui_Value[i], value_positions[i][1]);
        lv_obj_set_align(ui_Value[i], LV_ALIGN_CENTER);

        // Create transparent click zone
        create_transparent_click_zone(ui_Screen3, ui_Value[i], i + 1);
    }

    ui_RPM_Value = lv_label_create(ui_Screen3);
    lv_obj_set_width(ui_RPM_Value, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_RPM_Value, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_RPM_Value, 0);
    lv_obj_set_y(ui_RPM_Value, -127);
    lv_obj_set_align(ui_RPM_Value, LV_ALIGN_CENTER);
    lv_label_set_text(ui_RPM_Value, "0");
    lv_obj_set_style_text_color(ui_RPM_Value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_RPM_Value, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_RPM_Value, &ui_font_fugaz_28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_RPM_Label = lv_label_create(ui_Screen3);
    lv_obj_set_width(ui_RPM_Label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_RPM_Label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_RPM_Label, 0);
    lv_obj_set_y(ui_RPM_Label, -164);
    lv_obj_set_align(ui_RPM_Label, LV_ALIGN_CENTER);
    lv_label_set_text(ui_RPM_Label, "RPM");
    lv_obj_set_style_text_color(ui_RPM_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_RPM_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_RPM_Label, &ui_font_fugaz_14, LV_PART_MAIN | LV_STATE_DEFAULT);
  
    ui_Speed_Value = lv_label_create(ui_Screen3);
    lv_obj_set_width(ui_Speed_Value, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Speed_Value, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Speed_Value, LV_ALIGN_CENTER);
    lv_obj_set_x(ui_Speed_Value, 0);
    lv_obj_set_y(ui_Speed_Value, 30);
    lv_label_set_text(ui_Speed_Value, "0");
    lv_obj_set_style_text_color(ui_Speed_Value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Speed_Value, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Speed_Value, &ui_font_fugaz_56, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Kmh = lv_label_create(ui_Screen3);
    lv_obj_set_width(ui_Kmh, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Kmh, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Kmh, 37);
    lv_obj_set_y(ui_Kmh, 64);
    lv_obj_set_align(ui_Kmh, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Kmh, "k/mh");
    lv_obj_set_style_text_color(ui_Kmh, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Kmh, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Kmh, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Indicator_Left = lv_img_create(ui_Screen3);
    lv_img_set_src(ui_Indicator_Left, &ui_img_indicator_left_png);
    lv_obj_set_width(ui_Indicator_Left, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_Indicator_Left, LV_SIZE_CONTENT);    /// 30
    lv_obj_set_x(ui_Indicator_Left, -95);
    lv_obj_set_y(ui_Indicator_Left, -133);
    lv_obj_set_align(ui_Indicator_Left, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Indicator_Left, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Indicator_Left, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_opa(ui_Indicator_Left, 50, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Indicator_Right = lv_img_create(ui_Screen3);
    lv_img_set_src(ui_Indicator_Right, &ui_img_indicator_right_png);
    lv_obj_set_width(ui_Indicator_Right, LV_SIZE_CONTENT);   /// 32
    lv_obj_set_height(ui_Indicator_Right, LV_SIZE_CONTENT);    /// 30
    lv_obj_set_x(ui_Indicator_Right, 95);
    lv_obj_set_y(ui_Indicator_Right, -133);
    lv_obj_set_align(ui_Indicator_Right, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Indicator_Right, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Indicator_Right, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_opa(ui_Indicator_Right, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
  
	ui_Bar_1 = lv_bar_create(ui_Screen3);
    // Set default range if not configured
    if (values_config[BAR1_VALUE_ID - 1].bar_max <= values_config[BAR1_VALUE_ID - 1].bar_min) {
        values_config[BAR1_VALUE_ID - 1].bar_min = 0;
        values_config[BAR1_VALUE_ID - 1].bar_max = 100;
    }
    lv_bar_set_range(ui_Bar_1, 
        values_config[BAR1_VALUE_ID - 1].bar_min, 
        values_config[BAR1_VALUE_ID - 1].bar_max);
    lv_bar_set_value(ui_Bar_1, values_config[BAR1_VALUE_ID - 1].bar_min, LV_ANIM_OFF);
    lv_obj_set_width(ui_Bar_1, 300);
    lv_obj_set_height(ui_Bar_1, 30);
    lv_obj_set_x(ui_Bar_1, -240);
    lv_obj_set_y(ui_Bar_1, 209);
    lv_obj_set_align(ui_Bar_1, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_Bar_1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Bar_1, lv_color_hex(0x2e2f2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Bar_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Bar_1, lv_color_hex(0x2e2f2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Bar_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Bar_1, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui_Bar_1, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(ui_Bar_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Bar_1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Bar_1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Bar_1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Bar_1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_Bar_1, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Bar_1, lv_color_hex(0x19439a), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Bar_1, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_Bar_1_Label = lv_label_create(ui_Screen3);
    lv_obj_set_width(ui_Bar_1_Label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Bar_1_Label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Bar_1_Label, -240);
    lv_obj_set_y(ui_Bar_1_Label, 181);
    lv_obj_set_align(ui_Bar_1_Label, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Bar_1_Label, label_texts[BAR1_VALUE_ID - 1]);
    lv_obj_set_style_text_color(ui_Bar_1_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Bar_1_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Bar_1_Label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Bar_1_Label, &ui_font_fugaz_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_Bar_1_Label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Bar_1_Label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Bar_1_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Bar_1_Label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Bar_1_Label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Bar_1_Label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Bar_1_Label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	ui_Bar_2 = lv_bar_create(ui_Screen3);
    // Set default range if not configured
    if (values_config[BAR2_VALUE_ID - 1].bar_max <= values_config[BAR2_VALUE_ID - 1].bar_min) {
        values_config[BAR2_VALUE_ID - 1].bar_min = 0;
        values_config[BAR2_VALUE_ID - 1].bar_max = 100;
    }
    lv_bar_set_range(ui_Bar_2, 
        values_config[BAR2_VALUE_ID - 1].bar_min, 
        values_config[BAR2_VALUE_ID - 1].bar_max);
    lv_bar_set_value(ui_Bar_2, values_config[BAR2_VALUE_ID - 1].bar_min, LV_ANIM_OFF);
    lv_obj_set_width(ui_Bar_2, 300);
    lv_obj_set_height(ui_Bar_2, 30);
    lv_obj_set_x(ui_Bar_2, 240);
    lv_obj_set_y(ui_Bar_2, 209);
    lv_obj_set_align(ui_Bar_2, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_Bar_2, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Bar_2, lv_color_hex(0x2e2f2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Bar_2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Bar_2, lv_color_hex(0x2e2f2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Bar_2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Bar_2, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui_Bar_2, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(ui_Bar_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Bar_2, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Bar_2, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Bar_2, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Bar_2, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_Bar_2, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Bar_2, lv_color_hex(0x38FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Bar_2, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_Bar_2_Label = lv_label_create(ui_Screen3);
    lv_obj_set_width(ui_Bar_2_Label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Bar_2_Label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Bar_2_Label, 240);
    lv_obj_set_y(ui_Bar_2_Label, 181);
    lv_obj_set_align(ui_Bar_2_Label, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Bar_2_Label, label_texts[BAR2_VALUE_ID - 1]);
    lv_obj_set_style_text_color(ui_Bar_2_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Bar_2_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Bar_2_Label, &ui_font_fugaz_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_Bar_2_Label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Bar_2_Label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Bar_2_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Bar_2_Label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Bar_2_Label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Bar_2_Label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Bar_2_Label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Gear_Panel = lv_obj_create(ui_Screen3);
    lv_obj_set_width(ui_Gear_Panel, 90);
    lv_obj_set_height(ui_Gear_Panel, 90);
    lv_obj_set_x(ui_Gear_Panel, 0);
    lv_obj_set_y(ui_Gear_Panel, 180);
    lv_obj_set_align(ui_Gear_Panel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Gear_Panel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Gear_Panel, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Gear_Panel, lv_color_hex(0x2e2f2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Gear_Panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Gear_Panel, lv_color_hex(0x2e2f2e), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Gear_Panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Gear_Panel, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(ui_Gear_Panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(ui_Gear_Panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui_Gear_Panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(ui_Gear_Panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Gear_Label = lv_label_create(ui_Screen3);
    lv_obj_set_width(ui_Gear_Label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Gear_Label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Gear_Label, 0);
    lv_obj_set_y(ui_Gear_Label, 152);
    lv_obj_set_align(ui_Gear_Label, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Gear_Label, label_texts[GEAR_VALUE_ID - 1]); 
    lv_obj_set_style_text_color(ui_Gear_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Gear_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Gear_Label, &ui_font_fugaz_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_Gear_Label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Gear_Label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Gear_Label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Gear_Label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Gear_Label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_GEAR_Value = lv_label_create(ui_Screen3);
    lv_obj_set_width(ui_GEAR_Value, 115);
    lv_obj_set_height(ui_GEAR_Value, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_GEAR_Value, 10);
    lv_obj_set_y(ui_GEAR_Value, 198);
    lv_obj_set_align(ui_GEAR_Value, LV_ALIGN_CENTER);
    lv_label_set_text(ui_GEAR_Value, "1");
    lv_obj_set_style_text_color(ui_GEAR_Value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_GEAR_Value, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_GEAR_Value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_GEAR_Value, &ui_font_Manrope_54_BOLD, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_zoom(ui_GEAR_Value, 210, LV_PART_MAIN | LV_STATE_DEFAULT);
  
    create_transparent_click_zone(ui_Screen3, ui_RPM_Value, RPM_VALUE_ID);
	create_transparent_click_zone(ui_Screen3, ui_Speed_Value, SPEED_VALUE_ID);
	create_transparent_click_zone(ui_Screen3, ui_GEAR_Value, GEAR_VALUE_ID);
	create_transparent_click_zone(ui_Screen3, ui_Bar_1, BAR1_VALUE_ID);
	create_transparent_click_zone(ui_Screen3, ui_Bar_2, BAR2_VALUE_ID);

    ui_RDM_Border = lv_img_create(ui_Screen3);
    lv_img_set_src(ui_RDM_Border, &ui_img_rev_0_sticker_png);
    lv_obj_set_width(ui_RDM_Border, LV_SIZE_CONTENT);   /// 100
    lv_obj_set_height(ui_RDM_Border, LV_SIZE_CONTENT);    /// 38
    lv_obj_set_x(ui_RDM_Border, 0);
    lv_obj_set_y(ui_RDM_Border, -66);
    lv_obj_set_align(ui_RDM_Border, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_RDM_Border, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_RDM_Border, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
  
    lv_color_t saved_color = values_config[RPM_VALUE_ID - 1].rpm_bar_color;
    ui_RDM_Main = lv_img_create(ui_Screen3);
    lv_img_set_src(ui_RDM_Main, &ui_img_rev_0_sticker2_png);
    lv_obj_set_width(ui_RDM_Main, LV_SIZE_CONTENT);   /// 100
    lv_obj_set_height(ui_RDM_Main, LV_SIZE_CONTENT);    /// 38
    lv_obj_set_x(ui_RDM_Main, 0);
    lv_obj_set_y(ui_RDM_Main, -66);
    lv_obj_set_align(ui_RDM_Main, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_RDM_Main, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_RDM_Main, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_img_recolor(ui_RDM_Main, saved_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_RDM_Main, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
  
  
    ui_RDM_Logo_Text = lv_img_create(ui_Screen3);
    lv_img_set_src(ui_RDM_Logo_Text, &ui_img_rev_0_sticker3_png);
    lv_obj_set_width(ui_RDM_Logo_Text, LV_SIZE_CONTENT);   /// 107
    lv_obj_set_height(ui_RDM_Logo_Text, LV_SIZE_CONTENT);    /// 40
    lv_obj_set_x(ui_RDM_Logo_Text, 0);
    lv_obj_set_y(ui_RDM_Logo_Text, -65);
    lv_obj_set_align(ui_RDM_Logo_Text, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_RDM_Logo_Text, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_RDM_Logo_Text, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_add_event_cb(ui_RDM_Logo_Text, device_settings_longpress_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_flag(ui_RDM_Logo_Text, LV_OBJ_FLAG_CLICKABLE);  // Make sure it's clickable

    // Create timer for RPM color updates
    lv_timer_create(check_rpm_color_update, 500, NULL);

    // In ui_Screen3_screen_init(), replace all individual warning creation with:
    for(int i = 0; i < 8; i++) {
        // Create warning circle
        warning_circles[i] = lv_obj_create(ui_Screen3);
        lv_obj_set_width(warning_circles[i], 15);
        lv_obj_set_height(warning_circles[i], 15);
        lv_obj_set_x(warning_circles[i], warning_positions[i].x);
        lv_obj_set_y(warning_circles[i], warning_positions[i].y);
        lv_obj_set_align(warning_circles[i], LV_ALIGN_CENTER);
        lv_obj_clear_flag(warning_circles[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(warning_circles[i], 100, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(warning_circles[i], lv_color_hex(0x292C29), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(warning_circles[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(warning_circles[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);

// Create warning label
warning_labels[i] = lv_label_create(ui_Screen3);
lv_obj_set_width(warning_labels[i], 51);
lv_obj_set_height(warning_labels[i], 41);
lv_obj_set_x(warning_labels[i], warning_positions[i].x);
lv_obj_set_y(warning_labels[i], -112);
lv_obj_set_align(warning_labels[i], LV_ALIGN_CENTER);
lv_obj_add_flag(warning_labels[i], LV_OBJ_FLAG_HIDDEN);

// Use the saved configuration label if it exists, otherwise use default
const char* saved_label = warning_configs[i].label;
if (saved_label && saved_label[0] != '\0') {
    // If there's a saved label, use it
    lv_label_set_text(warning_labels[i], saved_label);
} else {
    // Use default label if no saved configuration
    char label_text[20];
    snprintf(label_text, sizeof(label_text), "Warning\n%d", i + 1);
    lv_label_set_text(warning_labels[i], label_text);
}

lv_obj_set_style_text_color(warning_labels[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
lv_obj_set_style_text_opa(warning_labels[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
lv_obj_set_style_text_align(warning_labels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
lv_obj_set_style_text_font(warning_labels[i], &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

warning_configs[i].current_state = false;

        // Create transparent touch area
        lv_obj_t * touch_area = lv_obj_create(ui_Screen3);
        lv_obj_set_size(touch_area, 50,80);
        lv_obj_set_x(touch_area, warning_positions[i].x);
        lv_obj_set_y(touch_area, warning_positions[i].y);
        lv_obj_set_align(touch_area, LV_ALIGN_CENTER);
        lv_obj_clear_flag(touch_area, LV_OBJ_FLAG_SCROLLABLE);
      
        // Make it transparent
        lv_obj_set_style_bg_opa(touch_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(touch_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      
        // Add long press handler
        uint8_t *warning_id = lv_mem_alloc(sizeof(uint8_t));
        *warning_id = i;
        lv_obj_add_event_cb(touch_area, warning_longpress_cb, LV_EVENT_LONG_PRESSED, warning_id);
    }

    // Store references if needed
    ui_Warning_1 = warning_circles[0];
    ui_Warning_2 = warning_circles[1];
    ui_Warning_3 = warning_circles[2];
    ui_Warning_4 = warning_circles[3];
    ui_Warning_5 = warning_circles[4];
    ui_Warning_6 = warning_circles[5];
    ui_Warning_7 = warning_circles[6];
    ui_Warning_8 = warning_circles[7];
}

