#include "ui_preconfig.h"
#include <stdio.h>
#include "lvgl.h"
#include "screens/ui_Screen3.h"
#include "widgets/lv_dropdown.h"

extern value_config_t values_config[13];
extern uint8_t current_value_id;

// Local static pointers to the Pre-configurations objects
static lv_obj_t * ui_Border_2 = NULL;
static lv_obj_t * ui_Preconfig_Text = NULL;
static lv_obj_t * ui_ECU_Text = NULL;
static lv_obj_t * ui_ECU_Input = NULL;
static lv_obj_t * ui_Version_Text = NULL;
static lv_obj_t * ui_Version_Input = NULL;
static lv_obj_t * ui_ID_Text = NULL;
static lv_obj_t * ui_ID_Input = NULL;

// A single “preconfig record”
typedef struct {
    const char* ecu;           // e.g. "MaxxECU"
    const char* version;       // e.g. "1.2"
    const char* label;         // e.g. "Lambda"
    uint32_t can_id;
    uint8_t endianess;         // 0 = Big Endian, 1 = Little Endian
    uint8_t bit_start;
    uint8_t bit_length;
    float scale;
    float value_offset;
    uint8_t decimals;
} preconfig_item_t;


static const preconfig_item_t preconfig_data[] = {
    { "MaxxECU", "1.2", "THROTTLE %", 520, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "MAP", 520, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "LAMBDA", 520, 1, 48, 16, 0.001, 0, 2 },
    { "MaxxECU", "1.2", "LAMBDA A", 521, 1, 0, 16, 0.001, 0, 2 },
    { "MaxxECU", "1.2", "LAMBDA B", 521, 1, 16, 16, 0.001, 0, 2 },
    { "MaxxECU", "1.2", "IGNITION ANGLE", 521, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "IGNITION CUT", 521, 1, 48, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "FUEL PULSEWIDTH PRIMARY", 522, 1, 0, 16, 0.01, 0, 0 },
    { "MaxxECU", "1.2", "FUEL DUTY PRIMARY", 522, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "FUEL CUT", 522, 1, 32, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "VEHICLE SPEED", 522, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "UNDRIVEN WHEELS AVG SPD", 523, 1, 0, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "DRIVEN WHEELS AVG SPD", 523, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "WHEEL SLIP", 523, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "TARGET SLIP", 523, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "TRACTION CTRL POWER LIMIT", 524, 1, 0, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "LAMBDA CORR A", 524, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "LAMBDA CORR B", 524, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "FIRMWARE VERSION", 524, 1, 48, 16, 0.001, 0, 0 },
    { "MaxxECU", "1.2", "BATTERY VOLTAGE", 530, 1, 0, 16, 0.01, 0, 0 },
    { "MaxxECU", "1.2", "BARO PRESSURE", 530, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "INTAKE AIR TEMP", 530, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "COOLANT TEMP", 530, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "TOTAL FUEL TRIM", 531, 1, 0, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "E85 %", 531, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "TOTAL IGNITION COMP", 531, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "EGT 1", 531, 1, 48, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "EGT 2", 532, 1, 0, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "EGT 3", 532, 1, 16, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "EGT 4", 532, 1, 32, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "EGT 5", 532, 1, 48, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "EGT 6", 533, 1, 0, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "EGT 7", 533, 1, 16, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "EGT 8", 533, 1, 32, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "EGT HIGHEST", 533, 1, 48, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "EGT DIFFERENCE", 534, 1, 0, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "CPU TEMP", 534, 1, 16, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "ERROR CODE COUNT", 534, 1, 32, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "LOST SYNC COUNT", 534, 1, 48, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "USER ANALOG INPUT 1", 535, 1, 0, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "USER ANALOG INPUT 2", 535, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "USER ANALOG INPUT 3", 535, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "USER ANALOG INPUT 4", 535, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.2", "GEAR", 536, 1, 0, 16, 1, 0, 0 },
    { "MaxxECU", "1.2", "BOOST SOLENOID DUTY", 536, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "OIL PRESSURE", 536, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "OIL TEMP", 536, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "FUEL PRESSURE 1", 537, 1, 0, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "WASTEGATE PRESSURE", 537, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "COOLANT PRESSURE", 537, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "BOOST TARGET", 537, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 1", 538, 1, 0, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 2", 538, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 3", 538, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 4", 538, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 5", 539, 1, 0, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 6", 539, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 7", 539, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 8", 539, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 9", 525, 1, 0, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 10", 525, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 11", 525, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "USER CHANNEL 12", 525, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "SHIFTCUT ACTIVE", 526, 1, 0, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "REV-LIMIT ACTIVE", 526, 1, 1, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "ANTI-LAG ACTIVE", 526, 1, 2, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "LAUNCH CONTROL ACTIVE", 526, 1, 3, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "TRACTION POWER LIMITER ACTIVE", 526, 1, 4, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "THROTTLE BLIP ACTIVE", 526, 1, 5, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "AC/IDLE UP ACTIVE", 526, 1, 6, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "KNOCK DETECTED", 526, 1, 7, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "BRAKE PEDAL ACTIVE", 526, 1, 8, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "CLUTCH PEDAL ACTIVE", 526, 1, 9, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "SPEED LIMIT ACTIVE", 526, 1, 10, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "GP LIMITER ACTIVE", 526, 1, 11, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "USER CUT ACTIVE", 526, 1, 12, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "ECU IS LOGGING", 526, 1, 13, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "NITROUS ACTIVE", 526, 1, 14, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "SPARE", 526, 1, 15, 1, 0, 0, 0 },
    { "MaxxECU", "1.3", "SPARE", 526, 1, 16, 16, 1, 0, 0 },
    { "MaxxECU", "1.3", "REV-LIMIT RPM", 526, 1, 32, 16, 1, 0, 0 },
    { "MaxxECU", "1.3", "SPARE", 526, 1, 48, 16, 1, 0, 0 },
    { "MaxxECU", "1.3", "ACCELERATION FORWARD", 527, 1, 0, 16, 0.01, 0, 0 },
    { "MaxxECU", "1.3", "ACCELERATION RIGHT", 527, 1, 16, 16, 0.01, 0, 0 },
    { "MaxxECU", "1.3", "ACCELERATION UP", 527, 1, 32, 16, 0.01, 0, 0 },
    { "MaxxECU", "1.3", "LAMBDA TARGET", 527, 1, 48, 16, 0.001, 0, 0 },
    { "MaxxECU", "1.3", "KNOCKLEVEL ALL PEAK", 528, 1, 0, 16, 1, 0, 0 },
    { "MaxxECU", "1.3", "KNOCK CORRECTION", 528, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "KNOCK COUNT", 528, 1, 32, 16, 1, 0, 0 },
    { "MaxxECU", "1.3", "LAST KNOCK CYLINDER", 528, 1, 48, 16, 1, 0, 0 },
    { "MaxxECU", "1.3", "ACTIVE BOOST TABLE", 540, 1, 0, 8, 1, 0, 0 },
    { "MaxxECU", "1.3", "ACTIVE TUNE SELECTOR", 540, 1, 8, 8, 1, 0, 0 },
    { "MaxxECU", "1.3", "VIRTUAL FUEL TANK", 540, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "TRANSMISSION TEMP", 540, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "DIFFERENTIAL TEMP", 540, 1, 48, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "VVT INTAKE CAM 1 POS", 541, 1, 0, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "VVT EXHAUST CAM 1 POS", 541, 1, 16, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "VVT INTAKE CAM 2 POS", 541, 1, 32, 16, 0.1, 0, 0 },
    { "MaxxECU", "1.3", "VVT EXHAUST CAM 2 POS", 541, 1, 48, 16, 0.1, 0, 0 }
};


static const int preconfig_data_count = sizeof(preconfig_data)/sizeof(preconfig_data[0]);


static void ecu_dropdown_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = lv_event_get_target(e);

        // 1) Get the new ECU selection text
        //    - If your dropdown has options like “Select ECU\nMaxxECU\nHaltech”,
        //      you can do something like:
        char selected_txt[64];
        lv_dropdown_get_selected_str(dropdown, selected_txt, sizeof(selected_txt));

        // 2) Based on “selected_txt”, we want to find which Versions exist in “preconfig_data”
        //    for this ECU. Then build a dynamic option string for “ui_Version_Input”.
        
        //    First, gather unique versions
        char versions[512] = {0};  // Enough room to store multiple lines
        bool first_version_added = false;
        
        for(int i = 0; i < preconfig_data_count; i++) {
            if (strcmp(preconfig_data[i].ecu, selected_txt) == 0) {
                // If not already in the string, add it
                if (!strstr(versions, preconfig_data[i].version)) {
                    // Add a newline if not the first
                    if (first_version_added) strcat(versions, "\n");
                    strcat(versions, preconfig_data[i].version);
                    first_version_added = true;
                }
            }
        }

        // 3) Update the version dropdown with these filtered items
        if (ui_Version_Input) {
            lv_dropdown_set_options(ui_Version_Input, versions);
            // Reset version selection to index 0
            lv_dropdown_set_selected(ui_Version_Input, 0);
        }

        // 4) Clear or reset the ID dropdown for now
        if (ui_ID_Input) {
            lv_dropdown_set_options(ui_ID_Input, ""); 
        }
    }
}

static void version_dropdown_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = lv_event_get_target(e);

        // 1) Figure out which ECU is selected
        char ecu_str[64];
        lv_dropdown_get_selected_str(ui_ECU_Input, ecu_str, sizeof(ecu_str));

        // 2) Figure out which version is selected
        char version_str[64];
        lv_dropdown_get_selected_str(dropdown, version_str, sizeof(version_str));

        // 3) Build a list of all “labels” for that ECU + version
        //    Then set them in the ID dropdown
        char id_labels[1024] = {0};  // Big buffer for all labels
        bool first_label_added = false;
        for(int i = 0; i < preconfig_data_count; i++) {
            if ((strcmp(preconfig_data[i].ecu, ecu_str) == 0) &&
                (strcmp(preconfig_data[i].version, version_str) == 0))
            {
                if (first_label_added) strcat(id_labels, "\n");
                strcat(id_labels, preconfig_data[i].label);
                first_label_added = true;
            }
        }

        if (ui_ID_Input) {
            lv_dropdown_set_options(ui_ID_Input, id_labels);
            lv_dropdown_set_selected(ui_ID_Input, 0);
        }
    }
}

static void id_dropdown_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    // 1) Gather ECU, version, label
    char ecu_str[64];
    lv_dropdown_get_selected_str(ui_ECU_Input, ecu_str, sizeof(ecu_str));

    char version_str[64];
    lv_dropdown_get_selected_str(ui_Version_Input, version_str, sizeof(version_str));

    lv_obj_t * dropdown = lv_event_get_target(e);
    char label_str[64];
    lv_dropdown_get_selected_str(dropdown, label_str, sizeof(label_str));

    // 2) Find the matching record
    const preconfig_item_t * found = NULL;
    for(int i = 0; i < preconfig_data_count; i++) {
        if ((strcmp(preconfig_data[i].ecu, ecu_str) == 0) &&
            (strcmp(preconfig_data[i].version, version_str) == 0) &&
            (strcmp(preconfig_data[i].label, label_str) == 0))
        {
            found = &preconfig_data[i];
            break;
        }
    }

    if (!found) return;

    // 3) Update the values_config
    uint8_t idx = (current_value_id <= 13) ? (current_value_id - 1) : 0;
    values_config[idx].can_id       = found->can_id;
    values_config[idx].endianess    = found->endianess;
    values_config[idx].bit_start    = found->bit_start;
    values_config[idx].bit_length   = found->bit_length;
    values_config[idx].scale        = found->scale;
    values_config[idx].value_offset = found->value_offset;
    values_config[idx].decimals     = found->decimals;
    values_config[idx].enabled      = true;

    // Also set the label_texts array
    strncpy(label_texts[idx], found->label, sizeof(label_texts[idx]) - 1);
    label_texts[idx][sizeof(label_texts[idx]) - 1] = '\0';

    // Print debug
    printf("Auto-populated panel #%d => label=%s, CAN=%u, bit_start=%d, bit_len=%d, scale=%.3f\n",
           current_value_id, found->label, found->can_id, found->bit_start,
           found->bit_length, found->scale);

    // 4) Update the actual UI text controls so the user sees the changes
    // Label
    if (g_label_input[idx]) {
        lv_textarea_set_text(g_label_input[idx], found->label);
    }

    // CAN ID
    if (g_can_id_input[idx]) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", found->can_id);
        lv_textarea_set_text(g_can_id_input[idx], buf);
    }

    // Endian dropdown (0 => Big, 1 => Little)
    if (g_endian_dropdown[idx]) {
        if (found->endianess == 0) {
            lv_dropdown_set_selected(g_endian_dropdown[idx], 0); // Big Endian
        } else {
            lv_dropdown_set_selected(g_endian_dropdown[idx], 1); // Little Endian
        }
    }

    // Bit start
    if (g_bit_start_dropdown[idx]) {
        lv_dropdown_set_selected(g_bit_start_dropdown[idx], found->bit_start);
    }

    // Bit length
    if (g_bit_length_dropdown[idx]) {
        lv_dropdown_set_selected(g_bit_length_dropdown[idx], (found->bit_length - 1));
    }

    // Scale
    if (g_scale_input[idx]) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.6g", found->scale);
        lv_textarea_set_text(g_scale_input[idx], buf);
    }

    // Offset
    if (g_offset_input[idx]) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.6g", found->value_offset);
        lv_textarea_set_text(g_offset_input[idx], buf);
    }

    // Decimals
    if (g_decimals_dropdown[idx]) {
        lv_dropdown_set_selected(g_decimals_dropdown[idx], found->decimals);
    }
}

void show_preconfig_menu(lv_obj_t * parent)
{
    if (ui_Border_2 != NULL) {
        // Menu already created, make it visible
        lv_obj_clear_flag(ui_Border_2, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // 2) Create a border or panel object (similar to your code)
    ui_Border_2 = lv_obj_create(parent);
    lv_obj_set_width(ui_Border_2, 373);
    lv_obj_set_height(ui_Border_2, 102);
    lv_obj_set_x(ui_Border_2, 201);
    lv_obj_set_y(ui_Border_2, -180);
    lv_obj_set_align(ui_Border_2, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Border_2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_Border_2, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Border_2, lv_color_hex(0x1A1A1A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Border_2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Border_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 3) Create "Pre-configurations" label
    ui_Preconfig_Text = lv_label_create(parent);
    lv_obj_set_width(ui_Preconfig_Text, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Preconfig_Text, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Preconfig_Text, 90);
    lv_obj_set_y(ui_Preconfig_Text, -216);
    lv_obj_set_align(ui_Preconfig_Text, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Preconfig_Text, "Pre-configurations");
    lv_obj_set_style_text_color(ui_Preconfig_Text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Preconfig_Text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 4) ECU label
    ui_ECU_Text = lv_label_create(parent);
    lv_obj_set_width(ui_ECU_Text, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ECU_Text, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ECU_Text, 39);
    lv_obj_set_y(ui_ECU_Text, -185);
    lv_obj_set_align(ui_ECU_Text, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ECU_Text, "ECU:");
    lv_obj_set_style_text_color(ui_ECU_Text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ECU_Text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 5) ECU dropdown
    ui_ECU_Input = lv_dropdown_create(parent);
    // Provide some default visible options for the user:
    lv_dropdown_set_options(ui_ECU_Input, "MaxxECU\nHaltech");
    lv_obj_add_event_cb(ui_ECU_Input, ecu_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL); 
    lv_obj_set_width(ui_ECU_Input, 109);
    lv_obj_set_height(ui_ECU_Input, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ECU_Input, 142);
    lv_obj_set_y(ui_ECU_Input, -185);
    lv_obj_set_align(ui_ECU_Input, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ECU_Input, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_style_radius(ui_ECU_Input, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_ECU_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_ECU_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_ECU_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_ECU_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 6) Version label
    ui_Version_Text = lv_label_create(parent);
    lv_obj_set_width(ui_Version_Text, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Version_Text, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Version_Text, 51);
    lv_obj_set_y(ui_Version_Text, -148);
    lv_obj_set_align(ui_Version_Text, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Version_Text, "Version:");
    lv_obj_set_style_text_color(ui_Version_Text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Version_Text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 7) Version dropdown
    ui_Version_Input = lv_dropdown_create(parent);
    // Provide a placeholder or default text
    lv_dropdown_set_options(ui_Version_Input, "Select Version");
    lv_obj_add_event_cb(ui_Version_Input, version_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_width(ui_Version_Input, 109);
    lv_obj_set_height(ui_Version_Input, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Version_Input, 142);
    lv_obj_set_y(ui_Version_Input, -148);
    lv_obj_set_align(ui_Version_Input, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Version_Input, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_style_radius(ui_Version_Input, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Version_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Version_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Version_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Version_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 8) ID label
    ui_ID_Text = lv_label_create(parent);
    lv_obj_set_width(ui_ID_Text, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ID_Text, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ID_Text, 227);
    lv_obj_set_y(ui_ID_Text, -185);
    lv_obj_set_align(ui_ID_Text, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ID_Text, "ID:");
    lv_obj_set_style_text_color(ui_ID_Text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ID_Text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 9) ID dropdown
    ui_ID_Input = lv_dropdown_create(parent);
    lv_dropdown_set_options(ui_ID_Input, "Select ID"); // placeholder
    lv_dropdown_set_dir(ui_ID_Input, LV_DIR_LEFT);
    lv_obj_add_event_cb(ui_ID_Input, id_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_width(ui_ID_Input, 122);
    lv_obj_set_height(ui_ID_Input, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ID_Input, 313);
    lv_obj_set_y(ui_ID_Input, -185);
    lv_obj_set_align(ui_ID_Input, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ID_Input, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_style_radius(ui_ID_Input, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_ID_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_ID_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_ID_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_ID_Input, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
}


void destroy_preconfig_menu(void)
{
    // If the objects exist, delete them
    if (ui_Border_2)       { lv_obj_del(ui_Border_2);       ui_Border_2       = NULL; }
    if (ui_Preconfig_Text) { lv_obj_del(ui_Preconfig_Text); ui_Preconfig_Text = NULL; }
    if (ui_ECU_Text)       { lv_obj_del(ui_ECU_Text);       ui_ECU_Text       = NULL; }
    if (ui_ECU_Input)      { lv_obj_del(ui_ECU_Input);      ui_ECU_Input      = NULL; }
    if (ui_Version_Text)   { lv_obj_del(ui_Version_Text);   ui_Version_Text   = NULL; }
    if (ui_Version_Input)  { lv_obj_del(ui_Version_Input);  ui_Version_Input  = NULL; }
    if (ui_ID_Text)        { lv_obj_del(ui_ID_Text);        ui_ID_Text        = NULL; }
    if (ui_ID_Input)       { lv_obj_del(ui_ID_Input);       ui_ID_Input       = NULL; }
}

