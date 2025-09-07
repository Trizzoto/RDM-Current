#include "create_config_controls.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "../screens/ui_Screen3.h"
#include "../ui.h"
#include "../ui_preconfig.h"

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

// External references to constants
#define RPM_VALUE_ID 9
#define SPEED_VALUE_ID 10
#define GEAR_VALUE_ID 11
#define BAR1_VALUE_ID 12
#define BAR2_VALUE_ID 13

// External references to callback functions
// These will be made non-static in ui_Screen3.c or declared as extern
extern void keyboard_event_cb(lv_event_t * e);
extern void label_input_event_cb(lv_event_t * e);
extern void can_id_input_event_cb(lv_event_t * e);
extern void endianess_roller_event_cb(lv_event_t * e);
extern void bit_start_roller_event_cb(lv_event_t * e);
extern void bit_length_roller_event_cb(lv_event_t * e);
extern void scale_input_event_cb(lv_event_t * e);
extern void value_offset_input_event_cb(lv_event_t * e);
extern void decimal_dropdown_event_cb(lv_event_t * e);
extern void type_dropdown_event_cb(lv_event_t * e);
extern void free_value_id_event_cb(lv_event_t * e);

// External references to styles and functions
extern void init_common_style(void);
extern lv_style_t* get_common_style(void);

// Placeholder for the function you will move over
void create_config_controls(lv_obj_t * parent, uint8_t value_id) {
    // Initialize the common style first
    init_common_style();
    
    // Get reference to the common style
    lv_style_t* common_style = get_common_style();
    
    // Base alignment: center
    lv_align_t base_align = LV_ALIGN_CENTER;
    int x_offset = 0;
    uint8_t idx = value_id - 1;

    printf("create_config_controls called with value_id: %d\n", value_id);
  
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
    lv_obj_add_style(label_input, common_style, LV_PART_MAIN);
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
    lv_obj_add_style(can_id_input, common_style, LV_PART_MAIN);
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
    lv_obj_add_style(endianess_dropdown, common_style, LV_PART_MAIN);
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
    lv_obj_add_style(bit_start_dropdown, common_style, LV_PART_MAIN);
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
    lv_obj_add_style(bit_length_dropdown, common_style, LV_PART_MAIN);
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
    lv_obj_add_style(scale_input, common_style, LV_PART_MAIN);
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
    lv_obj_add_style(value_offset_input, common_style, LV_PART_MAIN);
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
    lv_obj_add_style(decimal_dropdown, common_style, LV_PART_MAIN);
    lv_dropdown_set_options(decimal_dropdown, "0\n1\n2\n3");  // Added option for 3 decimal places
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
    lv_obj_add_style(type_dropdown, common_style, LV_PART_MAIN);
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