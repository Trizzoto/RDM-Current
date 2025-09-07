#include "ui_callbacks.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lvgl.h"
#include "../screens/ui_Screen3.h"
#include "../screens/ui_wifi.h"
#include "../ui.h"

// External references to global variables from ui_Screen3.c
extern lv_obj_t * g_label_input[];
extern lv_obj_t * g_can_id_input[];
extern lv_obj_t * g_endian_dropdown[];
extern lv_obj_t * g_bit_start_dropdown[];
extern lv_obj_t * g_bit_length_dropdown[];
extern lv_obj_t * g_scale_input[];
extern lv_obj_t * g_offset_input[];
extern lv_obj_t * g_decimals_dropdown[];
extern lv_obj_t * g_type_dropdown[];

extern value_config_t values_config[];
extern char label_texts[13][64];
extern lv_obj_t* ui_MenuScreen;
extern lv_obj_t* keyboard;

// External references to UI objects that may be used in callbacks
extern lv_obj_t* ui_Label[];
extern lv_obj_t* ui_Gear_Label;
extern lv_obj_t* ui_Bar_1_Label;
extern lv_obj_t* ui_Bar_2_Label;

// External references to constants
#define RPM_VALUE_ID 9
#define SPEED_VALUE_ID 10
#define GEAR_VALUE_ID 11
#define BAR1_VALUE_ID 12
#define BAR2_VALUE_ID 13

// External function references
extern void print_value_config(uint8_t value_id);

// Placeholder functions - you will move the actual implementations here
void label_input_event_cb(lv_event_t * e) {
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
            
            // Also update menu preview label if it exists and menu is visible
            if (value_id >= 1 && value_id <= 8) {
                extern lv_obj_t * menu_panel_labels[8];
                extern lv_obj_t * ui_MenuScreen;
                uint8_t idx = value_id - 1;
                if (menu_panel_labels[idx] && lv_obj_is_valid(menu_panel_labels[idx]) && 
                    ui_MenuScreen && lv_obj_is_valid(ui_MenuScreen) && lv_scr_act() == ui_MenuScreen) {
                    lv_label_set_text(menu_panel_labels[idx], txt);
                }
            }
        }
        // Handle bar labels
        else if (value_id == BAR1_VALUE_ID && ui_Bar_1_Label) {
            strncpy(label_texts[value_id - 1], txt, sizeof(label_texts[value_id - 1]));
            label_texts[value_id - 1][sizeof(label_texts[value_id - 1]) - 1] = '\0';
            lv_label_set_text(ui_Bar_1_Label, txt);
            
            // Also update menu preview bar label if it exists and menu is visible
            extern lv_obj_t * menu_bar_labels[2];
            extern lv_obj_t * ui_MenuScreen;
            if (menu_bar_labels[0] && lv_obj_is_valid(menu_bar_labels[0]) && 
                ui_MenuScreen && lv_obj_is_valid(ui_MenuScreen) && lv_scr_act() == ui_MenuScreen) {
                lv_label_set_text(menu_bar_labels[0], txt);
            }
        }
        else if (value_id == BAR2_VALUE_ID && ui_Bar_2_Label) {
            strncpy(label_texts[value_id - 1], txt, sizeof(label_texts[value_id - 1]));
            label_texts[value_id - 1][sizeof(label_texts[value_id - 1]) - 1] = '\0';
            lv_label_set_text(ui_Bar_2_Label, txt);
            
            // Also update menu preview bar label if it exists and menu is visible
            extern lv_obj_t * menu_bar_labels[2];
            extern lv_obj_t * ui_MenuScreen;
            if (menu_bar_labels[1] && lv_obj_is_valid(menu_bar_labels[1]) && 
                ui_MenuScreen && lv_obj_is_valid(ui_MenuScreen) && lv_scr_act() == ui_MenuScreen) {
                lv_label_set_text(menu_bar_labels[1], txt);
            }
        }
    }
}


void bit_start_roller_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = lv_event_get_target(e);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);

        if (value_id < 1 || value_id > 13) return;

        // Get the selected option index - these are dropdown widgets, not rollers
        uint8_t selected_bit_start = lv_dropdown_get_selected(dropdown);
        values_config[value_id - 1].bit_start = selected_bit_start;

        printf("Updated Bit Start for Value #%d to %d\n", value_id, selected_bit_start);
        print_value_config(value_id);
    }
}


void bit_length_roller_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = lv_event_get_target(e);
        uint8_t value_id = *(uint8_t *)lv_event_get_user_data(e);

        if (value_id < 1 || value_id > 13) return;

        // Get the selected option index and adjust for 1-based length - these are dropdown widgets, not rollers
        uint8_t selected_bit_length = lv_dropdown_get_selected(dropdown) + 1;
        values_config[value_id - 1].bit_length = selected_bit_length;

        printf("Updated Bit Length for Value #%d to %d\n", value_id, selected_bit_length);
        print_value_config(value_id);
    }
}

void decimal_dropdown_event_cb(lv_event_t * e) {
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

// Text Input Dialog Event Handlers
static text_input_dialog_t *current_text_dialog = NULL;

// Text Input Dialog Event Handlers
static void text_input_keyboard_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (!current_text_dialog) return;
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        // Update the text display with current keyboard input
        const char *text = lv_textarea_get_text(current_text_dialog->target_textarea);
        if (text && current_text_dialog->text_display) {
            lv_label_set_text(current_text_dialog->text_display, text);
            // Restore black color for actual text (not placeholder)
            lv_obj_set_style_text_color(current_text_dialog->text_display, lv_color_black(), 0);
        }
    }
    else if (code == LV_EVENT_READY) {
        // User pressed Enter/Done - confirm the input
        if (current_text_dialog) {
            const char *text = lv_textarea_get_text(current_text_dialog->target_textarea);
            
            // Call custom confirm callback if provided
            if (current_text_dialog->on_confirm) {
                current_text_dialog->on_confirm(text ? text : "", current_text_dialog->user_data);
            }
            
            // Trigger value changed event for the target textarea to update the system
            if (current_text_dialog->target_textarea && lv_obj_is_valid(current_text_dialog->target_textarea)) {
                lv_event_send(current_text_dialog->target_textarea, LV_EVENT_VALUE_CHANGED, NULL);
            }
        }
        close_text_input_dialog();
    }
    else if (code == LV_EVENT_CANCEL) {
        // User pressed Escape/Cancel
        if (current_text_dialog->on_cancel) {
            current_text_dialog->on_cancel(current_text_dialog->user_data);
        }
        close_text_input_dialog();
    }
}

static void text_input_ok_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    if (current_text_dialog) {
        const char *text = lv_textarea_get_text(current_text_dialog->target_textarea);
        
        // Call custom confirm callback if provided
        if (current_text_dialog->on_confirm) {
            current_text_dialog->on_confirm(text ? text : "", current_text_dialog->user_data);
        }
        
        // Trigger value changed event for the target textarea to update the system
        if (current_text_dialog->target_textarea && lv_obj_is_valid(current_text_dialog->target_textarea)) {
            lv_event_send(current_text_dialog->target_textarea, LV_EVENT_VALUE_CHANGED, NULL);
        }
    }
    close_text_input_dialog();
}

static void text_input_cancel_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    if (current_text_dialog && current_text_dialog->on_cancel) {
        current_text_dialog->on_cancel(current_text_dialog->user_data);
    }
    close_text_input_dialog();
}

void show_text_input_dialog_ex(lv_obj_t *target_textarea, const char *title, const char *placeholder, bool show_prefix,
                              void (*on_confirm)(const char *text, void *user_data),
                              void (*on_cancel)(void *user_data), void *user_data) {
    
    // Check if we're on the WiFi screen - don't create dialog
    if (is_wifi_screen_active()) {
        return;
    }
    
    // Validate target textarea
    if (!target_textarea || !lv_obj_is_valid(target_textarea)) {
        return;
    }
    
    // Close any existing dialog
    if (current_text_dialog) {
        close_text_input_dialog();
    }
    
    // Allocate dialog structure
    current_text_dialog = lv_mem_alloc(sizeof(text_input_dialog_t));
    if (!current_text_dialog) return;
    
    memset(current_text_dialog, 0, sizeof(text_input_dialog_t));
    current_text_dialog->target_textarea = target_textarea;
    current_text_dialog->on_confirm = on_confirm;
    current_text_dialog->on_cancel = on_cancel;
    current_text_dialog->user_data = user_data;
    
    // Create modal background on current screen (not top layer) for screenshots
    current_text_dialog->modal = lv_obj_create(lv_scr_act());
    lv_obj_set_size(current_text_dialog->modal, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(current_text_dialog->modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(current_text_dialog->modal, LV_OPA_70, 0);
    lv_obj_clear_flag(current_text_dialog->modal, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create dialog container - taller to fully contain buttons
    lv_obj_t *dialog_container = lv_obj_create(current_text_dialog->modal);
    lv_obj_set_size(dialog_container, 400, 160);
    lv_obj_set_style_bg_color(dialog_container, lv_color_hex(0x2e2f2e), 0);
    lv_obj_set_style_bg_opa(dialog_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dialog_container, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(dialog_container, 2, 0);
    lv_obj_set_style_radius(dialog_container, 8, 0);
    lv_obj_align(dialog_container, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_clear_flag(dialog_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title label - smaller font
    if (title) {
        lv_obj_t *title_label = lv_label_create(dialog_container);
        lv_label_set_text(title_label, title);
        lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 8);
    }
    
    // Create prefix label if needed (for CAN ID: 0x)
    if (show_prefix) {
        current_text_dialog->prefix_label = lv_label_create(dialog_container);
        lv_label_set_text(current_text_dialog->prefix_label, "0x");
        lv_obj_set_style_text_color(current_text_dialog->prefix_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(current_text_dialog->prefix_label, &lv_font_montserrat_14, 0);
        lv_obj_align(current_text_dialog->prefix_label, LV_ALIGN_TOP_MID, -130, 47);
    } else {
        current_text_dialog->prefix_label = NULL;
    }
    
    // Text display area (shows what user is typing) - white background like config menu
    current_text_dialog->text_display = lv_label_create(dialog_container);
    int display_width = show_prefix ? 260 : 300;  // Narrower if prefix is shown
    int display_x = show_prefix ? 20 : 0;          // Offset to right if prefix is shown
    lv_obj_set_size(current_text_dialog->text_display, display_width, 30);
    lv_obj_set_style_bg_color(current_text_dialog->text_display, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(current_text_dialog->text_display, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(current_text_dialog->text_display, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(current_text_dialog->text_display, 1, 0);
    lv_obj_set_style_radius(current_text_dialog->text_display, 4, 0);
    lv_obj_set_style_text_color(current_text_dialog->text_display, lv_color_black(), 0);
    lv_obj_set_style_text_font(current_text_dialog->text_display, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_all(current_text_dialog->text_display, 6, 0);
    lv_obj_align(current_text_dialog->text_display, LV_ALIGN_TOP_MID, display_x, 40);
    lv_label_set_long_mode(current_text_dialog->text_display, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    // Set initial text or placeholder
    if (target_textarea) {
        const char *current_text = lv_textarea_get_text(target_textarea);
        if (current_text && strlen(current_text) > 0) {
            lv_label_set_text(current_text_dialog->text_display, current_text);
            // Keep black text for existing content
            lv_obj_set_style_text_color(current_text_dialog->text_display, lv_color_black(), 0);
        } else if (placeholder) {
            lv_label_set_text(current_text_dialog->text_display, placeholder);
            // Grey color for placeholder text
            lv_obj_set_style_text_color(current_text_dialog->text_display, lv_color_hex(0x888888), 0);
        }
    }
    
    // Button container - positioned with more space in taller dialog
    lv_obj_t *btn_container = lv_obj_create(dialog_container);
    lv_obj_set_size(btn_container, 300, 35);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_align(btn_container, LV_ALIGN_TOP_MID, 0, 105);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Cancel button - smaller
    lv_obj_t *cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(cancel_btn, 120, 30);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xF44336), 0);
    lv_obj_set_style_radius(cancel_btn, 6, 0);
    lv_obj_add_event_cb(cancel_btn, text_input_cancel_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_color(cancel_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_12, 0);
    lv_obj_center(cancel_label);
    
    // OK button - smaller
    lv_obj_t *ok_btn = lv_btn_create(btn_container);
    lv_obj_set_size(ok_btn, 120, 30);
    lv_obj_set_style_bg_color(ok_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_radius(ok_btn, 6, 0);
    lv_obj_add_event_cb(ok_btn, text_input_ok_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *ok_label = lv_label_create(ok_btn);
    lv_label_set_text(ok_label, "OK");
    lv_obj_set_style_text_color(ok_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(ok_label, &lv_font_montserrat_12, 0);
    lv_obj_center(ok_label);
    
    // Create keyboard
    current_text_dialog->keyboard = lv_keyboard_create(current_text_dialog->modal);
    lv_obj_set_style_bg_color(current_text_dialog->keyboard, lv_color_hex(0x303030), 0);
    lv_obj_set_style_bg_opa(current_text_dialog->keyboard, LV_OPA_COVER, 0);
    lv_obj_align(current_text_dialog->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Connect keyboard to target textarea
    if (target_textarea) {
        lv_keyboard_set_textarea(current_text_dialog->keyboard, target_textarea);
        lv_obj_add_event_cb(current_text_dialog->keyboard, text_input_keyboard_event_cb, LV_EVENT_ALL, NULL);
        
        // Focus the textarea to show cursor
        lv_obj_add_state(target_textarea, LV_STATE_FOCUSED);
    }
}

void close_text_input_dialog(void) {
    if (!current_text_dialog) return;
    
    // Store reference to avoid using freed memory
    text_input_dialog_t *dialog_to_close = current_text_dialog;
    current_text_dialog = NULL;  // Clear global reference first
    
    // Clean up modal and all child objects
    if (dialog_to_close->modal && lv_obj_is_valid(dialog_to_close->modal)) {
        lv_obj_del(dialog_to_close->modal);
    }
    
    // Clear textarea focus if it exists
    if (dialog_to_close->target_textarea && lv_obj_is_valid(dialog_to_close->target_textarea)) {
        lv_obj_clear_state(dialog_to_close->target_textarea, LV_STATE_FOCUSED);
    }
    
    // Free the dialog structure
    lv_mem_free(dialog_to_close);
}

// Force close any active text input dialog - useful for screen transitions
void force_close_text_input_dialog(void) {
    if (current_text_dialog) {
        close_text_input_dialog();
    }
}

// Wrapper function for backward compatibility
void show_text_input_dialog(lv_obj_t *target_textarea, const char *title, const char *placeholder,
                           void (*on_confirm)(const char *text, void *user_data),
                           void (*on_cancel)(void *user_data), void *user_data) {
    show_text_input_dialog_ex(target_textarea, title, placeholder, false, on_confirm, on_cancel, user_data);
}

// Updated keyboard_event_cb to use the new text input dialog
void keyboard_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_FOCUSED) {
        if(obj == NULL) return;
        
        // Check if we're on the WiFi screen - if so, skip our custom dialog
        // The WiFi screen handles its own keyboard with password_input_event_cb
        if (is_wifi_screen_active()) {
            return;
        }
        
        // For all other screens, show our custom text input dialog with context detection
        const char *title = "Enter Text";
        const char *placeholder = "Type here...";
        bool show_prefix = false;
        
        // Detect field type based on placeholder or position
        const char *existing_placeholder = lv_textarea_get_placeholder_text(obj);
        if (existing_placeholder) {
            if (strstr(existing_placeholder, "CAN ID") || strstr(existing_placeholder, "Enter CAN ID")) {
                title = "CAN ID:";
                placeholder = "530";
                show_prefix = true;
            } else if (strstr(existing_placeholder, "Label") || strstr(existing_placeholder, "Enter Label")) {
                title = "Label:";
                placeholder = "Enter Label";
            } else if (strstr(existing_placeholder, "Scale") || strstr(existing_placeholder, "Enter Scale")) {
                title = "Scale:";
                placeholder = "1.0";
            } else if (strstr(existing_placeholder, "Offset") || strstr(existing_placeholder, "Enter Offset")) {
                title = "Offset:";
                placeholder = "0.0";
            }
        }
        
        show_text_input_dialog_ex(obj, title, placeholder, show_prefix, NULL, NULL, NULL);
        
    } else if(code == LV_EVENT_DEFOCUSED) {
        // Check if we're on the WiFi screen
        if (is_wifi_screen_active()) {
            return;
        }
        
        // For all other screens, close the text input dialog
        close_text_input_dialog();
    }
}

void free_value_id_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        uint8_t * p_id = (uint8_t *)lv_event_get_user_data(e);
        if (p_id) {
            lv_mem_free(p_id);  // Only free if it's dynamically allocated
        }
    }
}


void endianess_roller_event_cb(lv_event_t * e) {
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


void value_offset_input_event_cb(lv_event_t * e) {
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


void can_id_input_event_cb(lv_event_t * e) {
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


void scale_input_event_cb(lv_event_t * e) {
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

void type_dropdown_event_cb(lv_event_t * e) {
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

