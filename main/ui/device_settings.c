#include "device_settings.h"
#include "lvgl.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "ui_wifi.h"
#include "ota_handler.h"
#include "nvs_flash.h"
#include "version.h"
#include "device_id.h"
#include "ui.h"
#include "ui_helpers.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "ota_update_dialog.h"
#include "lwip/ip4_addr.h"

// External variables
extern TaskHandle_t canTaskHandle;
extern twai_timing_config_t g_t_config;
extern twai_general_config_t g_config;
extern twai_filter_config_t f_config;
extern volatile bool can_task_should_stop;
extern void can_receive_task(void *pvParameter);
extern char* connected_ssid;

// Global WiFi status labels for updating
static lv_obj_t* wifi_status_label = NULL;
static lv_obj_t* web_status_label = NULL;

// Function to refresh WiFi status displays
static void refresh_wifi_status(void) {
    if (!wifi_status_label || !web_status_label) return;
    
    // Update WiFi status
    if (connected_ssid) {
        char status_text[32];
        snprintf(status_text, sizeof(status_text), "WiFi: %s", connected_ssid);
        lv_label_set_text(wifi_status_label, status_text);
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x00FF80), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_label_set_text(wifi_status_label, "WiFi: Not Connected");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF4080), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    
    // Update web server status
    if (connected_ssid) {
        // Get IP address
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char web_text[64];
            snprintf(web_text, sizeof(web_text), "Web: http://" IPSTR, IP2STR(&ip_info.ip));
            lv_label_set_text(web_status_label, web_text);
            lv_obj_set_style_text_color(web_status_label, lv_color_hex(0x4080FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_label_set_text(web_status_label, "Web: Waiting for IP...");
            lv_obj_set_style_text_color(web_status_label, lv_color_hex(0xFFFF40), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    } else {
        lv_label_set_text(web_status_label, "Web: Connect WiFi first");
        lv_obj_set_style_text_color(web_status_label, lv_color_hex(0xFF4080), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

// Timer callback wrapper for WiFi status refresh
static void refresh_wifi_status_timer_cb(lv_timer_t* timer) {
    refresh_wifi_status();
}

// LEDC configuration defines
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          16 // GPIO16
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // 13 bits
#define LEDC_FREQUENCY          5000 // 5 kHz

// Static variables
static bool ledc_initialized = false;
static lv_obj_t* brightness_label = NULL;
static uint8_t selected_ecu_preconfig = 0; // 0=Custom, 1=MaxxECU, 2=Haltech
static uint8_t selected_ecu_version = 0; // For storing selected version index
static uint8_t current_brightness = 100; // Track current brightness value
static lv_obj_t* device_settings_return_screen = NULL; // Screen to return to when closing device settings

void set_display_brightness(int percent) {
    if (percent < 5) percent = 5;
    if (percent > 100) percent = 100;
    
    // Track current brightness value
    current_brightness = percent;

    if (!ledc_initialized) {
        ledc_timer_config_t ledc_timer = {
            .speed_mode       = LEDC_MODE,
            .timer_num        = LEDC_TIMER,
            .duty_resolution  = LEDC_DUTY_RES,
            .freq_hz          = LEDC_FREQUENCY,
            .clk_cfg          = LEDC_AUTO_CLK
        };
        ledc_timer_config(&ledc_timer);

        ledc_channel_config_t ledc_channel = {
            .speed_mode     = LEDC_MODE,
            .channel        = LEDC_CHANNEL,
            .timer_sel      = LEDC_TIMER,
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = LEDC_OUTPUT_IO,
            .duty           = 0,
            .hpoint         = 0
        };
        ledc_channel_config(&ledc_channel);
        ledc_initialized = true;
    }

    // Map percent (5-100) to duty (0-8191 for 13 bits)
    uint32_t duty = (uint32_t)((percent / 100.0f) * ((1 << 13) - 1));
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

// Callback to update brightness
static void brightness_bar_event_cb(lv_event_t * e) {
    lv_obj_t * bar = lv_event_get_target(e);
    int val = lv_slider_get_value(bar);
    set_display_brightness(val);
    
    // Update label only - don't save to NVS
    if (brightness_label) {
        lv_label_set_text_fmt(brightness_label, "Brightness: %d%%", val);
    }
}

// Close menu callback
static void close_menu_event_cb(lv_event_t * e) {
    lv_obj_t * old_screen = (lv_obj_t *)lv_event_get_user_data(e);
    if (old_screen) {
        lv_scr_load(old_screen);
    }
}

// Bitrate dropdown callback
static void bitrate_dropdown_event_cb(lv_event_t * e) {
    lv_obj_t * dd = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dd);
    esp_err_t err;
    
    ESP_LOGI("CAN", "Auto-applying bitrate change to option %d", selected);
    
    // Auto-save bitrate setting to NVS
    nvs_handle_t handle;
    if (nvs_open("can_config", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "can_bitrate", selected);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI("CAN", "Bitrate setting saved to NVS");
    }
    
    // Configure CAN timing based on selection
    switch(selected) {
        case 0: // 125 kbps
            g_t_config.brp = 64;
            g_t_config.tseg_1 = 15;
            g_t_config.tseg_2 = 4;
            g_t_config.sjw = 3;
            ESP_LOGI("CAN", "Configured for 125 kbps");
            break;
        case 1: // 250 kbps
            g_t_config.brp = 32;
            g_t_config.tseg_1 = 15;
            g_t_config.tseg_2 = 4;
            g_t_config.sjw = 3;
            ESP_LOGI("CAN", "Configured for 250 kbps");
            break;
        case 2: // 500 kbps
            g_t_config.brp = 16;
            g_t_config.tseg_1 = 15;
            g_t_config.tseg_2 = 4;
            g_t_config.sjw = 3;
            ESP_LOGI("CAN", "Configured for 500 kbps");
            break;
        case 3: // 1 Mbps
            g_t_config.brp = 8;
            g_t_config.tseg_1 = 15;
            g_t_config.tseg_2 = 4;
            g_t_config.sjw = 3;
            ESP_LOGI("CAN", "Configured for 1 Mbps");
            break;
        default:
            ESP_LOGE("CAN", "Invalid bitrate selection: %d", selected);
            return;
    }
    
    // Step 1: Signal the CAN task to stop gracefully
    if (canTaskHandle != NULL) {
        ESP_LOGI("CAN", "Signaling CAN task to stop gracefully");
        can_task_should_stop = true;
        
        // Wait for the task to exit (up to 2 seconds)
        for (int i = 0; i < 200; i++) {
            if (eTaskGetState(canTaskHandle) == eDeleted) {
                ESP_LOGI("CAN", "CAN task exited gracefully");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // If task still exists after timeout, force delete it
        if (eTaskGetState(canTaskHandle) != eDeleted) {
            ESP_LOGW("CAN", "Force deleting CAN task after timeout");
            vTaskDelete(canTaskHandle);
        }
        canTaskHandle = NULL;
    }

    // Step 2: Stop and uninstall CAN driver
    ESP_LOGI("CAN", "Stopping CAN driver");
    err = twai_stop();
    if (err != ESP_OK) {
        ESP_LOGE("CAN", "Failed to stop CAN driver: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI("CAN", "Uninstalling CAN driver");
    err = twai_driver_uninstall();
    if (err != ESP_OK) {
        ESP_LOGE("CAN", "Failed to uninstall CAN driver: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // Step 3: Reinstall driver with new configuration
    ESP_LOGI("CAN", "Installing CAN driver with new configuration");
    err = twai_driver_install(&g_config, &g_t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE("CAN", "Failed to install CAN driver: %s", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // Step 4: Start the driver
    ESP_LOGI("CAN", "Starting CAN driver");
    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE("CAN", "Failed to start CAN driver: %s", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // Step 5: Recreate the CAN task
    ESP_LOGI("CAN", "Recreating CAN receive task");
    if (xTaskCreatePinnedToCore(can_receive_task, "can_receive_task", 
                               4096, NULL, 7, &canTaskHandle, 0) != pdPASS) {
        ESP_LOGE("CAN", "Failed to create CAN receive task");
        canTaskHandle = NULL;
        return;
    }

    ESP_LOGI("CAN", "Bitrate change completed successfully");
}

// Loading dialog for WiFi
static lv_obj_t* wifi_loading_dialog = NULL;

// Timer callback to show WiFi screen after loading dialog
static void show_wifi_screen_delayed(lv_timer_t* timer) {
    // Close loading dialog
    if (wifi_loading_dialog && lv_obj_is_valid(wifi_loading_dialog)) {
        lv_obj_del(wifi_loading_dialog);
        wifi_loading_dialog = NULL;
    }
    
    // Show WiFi screen
    show_wifi_screen();
    
    // Delete the timer
    lv_timer_del(timer);
}

// WiFi button callback
static void wifi_btn_event_cb(lv_event_t *e) {
    // Create loading dialog immediately
    wifi_loading_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(wifi_loading_dialog, 300, 150);
    lv_obj_align(wifi_loading_dialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(wifi_loading_dialog, lv_color_hex(0x2E2E2E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(wifi_loading_dialog, lv_color_hex(0x4080FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(wifi_loading_dialog, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(wifi_loading_dialog, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(wifi_loading_dialog, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(wifi_loading_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Loading title
    lv_obj_t* title = lv_label_create(wifi_loading_dialog);
    lv_label_set_text(title, "Wi-Fi Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Loading spinner
    lv_obj_t* spinner = lv_spinner_create(wifi_loading_dialog, 1000, 60);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x4080FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x4080FF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    
    // Loading text
    lv_obj_t* loading_text = lv_label_create(wifi_loading_dialog);
    lv_label_set_text(loading_text, "Searching for networks...");
    lv_obj_align(loading_text, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_color(loading_text, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(loading_text, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Create timer to show WiFi screen after a brief delay (allows dialog to render)
    lv_timer_create(show_wifi_screen_delayed, 100, NULL);
}

// Update button callback
static void update_btn_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    ESP_LOGI("OTA", "Check for updates button clicked");
    
    // First check for updates
    check_for_update();
    
    // Check the result and show appropriate dialog
    ota_status_t status = get_ota_status();
    switch (status) {
        case OTA_UPDATE_AVAILABLE: {
            ESP_LOGI("OTA", "Update available, showing dialog");
            
            // Get update information from OTA handler
            const char* current_version = FIRMWARE_VERSION;
            const char* new_version = get_latest_version();
            const char* update_type = get_update_type_str();
            float file_size_mb = get_update_file_size_mb();
            const char* release_notes = get_release_notes();
            
            // Show the OTA update dialog
            show_ota_update_dialog(current_version, new_version, update_type, file_size_mb, release_notes);
            break;
        }
        case OTA_NO_UPDATE_AVAILABLE:
            ESP_LOGI("OTA", "No update available");
            // Could show a "No updates available" message
            break;
        case OTA_UPDATE_FAILED:
            ESP_LOGE("OTA", "Update check failed");
            // Could show an error message
            break;
        default:
            ESP_LOGW("OTA", "Unexpected OTA status: %d", status);
            break;
    }
}

// Forward declarations
static void update_ecu_status_label(lv_obj_t* status_label);

// Structure to pass multiple objects to ECU dropdown callback
typedef struct {
    lv_obj_t* version_dropdown;
    lv_obj_t* status_label;
} ecu_callback_data_t;

// ECU dropdown callback
static void ecu_dropdown_event_cb(lv_event_t *e) {
    lv_obj_t * dd = lv_event_get_target(e);
    selected_ecu_preconfig = lv_dropdown_get_selected(dd);
    ESP_LOGI("ECU", "ECU preconfig selected: %d", selected_ecu_preconfig);
    
    // Get the callback data from user data
    ecu_callback_data_t* data = (ecu_callback_data_t*)lv_event_get_user_data(e);
    if (data && data->version_dropdown) {
        // Update version dropdown options based on selected ECU
        selected_ecu_version = 0; // Reset to first option
        switch (selected_ecu_preconfig) {
            case 0: // Custom
                lv_dropdown_set_options(data->version_dropdown, "Select ECU First");
                break;
            case 1: // MaxxECU
                lv_dropdown_set_options(data->version_dropdown, "1.2\n1.3");
                lv_dropdown_set_selected(data->version_dropdown, 0);
                break;
            case 2: // Haltech
                lv_dropdown_set_options(data->version_dropdown, "3.10");
                lv_dropdown_set_selected(data->version_dropdown, 0);
                break;
        }
    }
    
    // Auto-save ECU preconfig to NVS immediately
    nvs_handle_t handle;
    if (nvs_open("ecu_config", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "ecu_preconfig", selected_ecu_preconfig);
        nvs_set_u8(handle, "ecu_version", selected_ecu_version);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI("ECU", "ECU preconfig auto-saved: ECU=%d, Version=%d", selected_ecu_preconfig, selected_ecu_version);
    }
    
    // Update status label
    if (data && data->status_label) {
        update_ecu_status_label(data->status_label);
    }
}

// Version dropdown callback
static void version_dropdown_event_cb(lv_event_t *e) {
    lv_obj_t * dd = lv_event_get_target(e);
    selected_ecu_version = lv_dropdown_get_selected(dd);
    ESP_LOGI("ECU", "ECU version selected: %d", selected_ecu_version);
    
    // Auto-save version selection to NVS immediately
    nvs_handle_t handle;
    if (nvs_open("ecu_config", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "ecu_preconfig", selected_ecu_preconfig);
        nvs_set_u8(handle, "ecu_version", selected_ecu_version);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI("ECU", "ECU version auto-saved: ECU=%d, Version=%d", selected_ecu_preconfig, selected_ecu_version);
    }
    
    // Update status label
    lv_obj_t* status_label = (lv_obj_t*)lv_event_get_user_data(e);
    if (status_label) {
        update_ecu_status_label(status_label);
    }
}



// Load ECU preconfig from NVS
void load_ecu_preconfig(void) {
    nvs_handle_t handle;
    if (nvs_open("ecu_config", NVS_READONLY, &handle) == ESP_OK) {
        if (nvs_get_u8(handle, "ecu_preconfig", &selected_ecu_preconfig) != ESP_OK) {
            selected_ecu_preconfig = 0; // Default to Custom
        }
        
        if (nvs_get_u8(handle, "ecu_version", &selected_ecu_version) != ESP_OK) {
            selected_ecu_version = 0; // Default to first version
        }
        
        nvs_close(handle);
        ESP_LOGI("ECU", "Loaded ECU preconfig from NVS: ECU=%d, Version=%d", selected_ecu_preconfig, selected_ecu_version);
    } else {
        // Set defaults if NVS can't be opened
        selected_ecu_preconfig = 0;
        selected_ecu_version = 0;
        ESP_LOGI("ECU", "Using default ECU preconfig: ECU=%d, Version=%d", selected_ecu_preconfig, selected_ecu_version);
    }
}

// Getter function for other files to access the selected ECU preconfig
uint8_t get_selected_ecu_preconfig(void) {
    return selected_ecu_preconfig;
}

// Getter function for the selected ECU version
uint8_t get_selected_ecu_version(void) {
    return selected_ecu_version;
}

// Function to update the ECU status label
static void update_ecu_status_label(lv_obj_t* status_label) {
    if (!status_label) return;
    
    char status_text[128];
    if (selected_ecu_preconfig == 0) {
        snprintf(status_text, sizeof(status_text), "[OK] Custom ECU preconfig applied");
    } else if (selected_ecu_preconfig == 1) {
        const char* version_str = (selected_ecu_version == 0) ? "1.2" : "1.3";
        snprintf(status_text, sizeof(status_text), "[OK] MaxxECU %s preconfig applied", version_str);
    } else if (selected_ecu_preconfig == 2) {
        snprintf(status_text, sizeof(status_text), "[OK] Haltech 3.10 preconfig applied");
    } else {
        snprintf(status_text, sizeof(status_text), "[OK] ECU preconfig applied");
    }
    
    lv_label_set_text(status_label, status_text);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF80), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void init_display_brightness(void) {
    // Always boot at 100% brightness
    set_display_brightness(100);
}

// Function to create device settings with a specific return screen
void device_settings_with_return_screen(lv_obj_t* return_screen) {
    // Load ECU preconfig from NVS
    load_ecu_preconfig();
    
    // Store the return screen for later use
    device_settings_return_screen = return_screen ? return_screen : lv_scr_act();
    lv_obj_t* settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(settings_screen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Main container with border - made taller
    lv_obj_t* main_container = lv_obj_create(settings_screen);
    lv_obj_set_size(main_container, 780, 460);
    lv_obj_align(main_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(main_container, lv_color_hex(0x1A1A1A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(main_container, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(main_container, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(main_container, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(main_container, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(main_container, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);

    // Header section
    lv_obj_t* header = lv_obj_create(main_container);
    lv_obj_set_size(header, lv_pct(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x2E2F2E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(header, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(header, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "DEVICE SETTINGS");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Close button
    lv_obj_t* close_btn = lv_btn_create(header);
    lv_obj_set_size(close_btn, 60, 40);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFF4444), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFF6666), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(close_btn, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(close_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t* close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "X");
    lv_obj_center(close_label);
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(close_btn, close_menu_event_cb, LV_EVENT_CLICKED, device_settings_return_screen);
    
    // Content area - made scrollable and taller to accommodate all sections
    lv_obj_t* content = lv_obj_create(main_container);
    lv_obj_set_size(content, lv_pct(100), 400);  // Increased height to fit all sections properly
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_opa(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(content, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(content, 25, LV_PART_MAIN | LV_STATE_DEFAULT);  // Extra right padding for scrollbar space
    lv_obj_set_scroll_dir(content, LV_DIR_VER);  // Enable vertical scrolling
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Scrollbar styling - move it away from content
    lv_obj_set_style_bg_color(content, lv_color_hex(0x555555), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(content, 200, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_width(content, 8, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(content, 5, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);  // Move scrollbar away from edge

    // First row container
    lv_obj_t* first_row = lv_obj_create(content);
    lv_obj_set_size(first_row, lv_pct(100), 130);
    lv_obj_set_style_bg_opa(first_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(first_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(first_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(first_row, 15, LV_PART_MAIN | LV_STATE_DEFAULT);  // Manual spacing
    lv_obj_clear_flag(first_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(first_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(first_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // CAN Bus Configuration Section
    lv_obj_t* can_section = lv_obj_create(first_row);
    lv_obj_set_size(can_section, lv_pct(48), 130);
    lv_obj_set_style_bg_color(can_section, lv_color_hex(0x262626), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(can_section, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(can_section, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(can_section, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(can_section, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(can_section, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(can_section, LV_OBJ_FLAG_SCROLLABLE);

    // CAN section title
    lv_obj_t* can_title = lv_label_create(can_section);
    lv_label_set_text(can_title, "CAN BUS");
    lv_obj_align(can_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(can_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(can_title, lv_color_hex(0x00FF80), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Bitrate label
    lv_obj_t* bitrate_label = lv_label_create(can_section);
    lv_label_set_text(bitrate_label, "Bitrate");
    lv_obj_align(bitrate_label, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_style_text_font(bitrate_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(bitrate_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Bitrate dropdown
    lv_obj_t* bitrate_dd = lv_dropdown_create(can_section);
    lv_dropdown_set_options(bitrate_dd, "125 kbps\n250 kbps\n500 kbps\n1 Mbps");
    lv_obj_set_size(bitrate_dd, 140, 35);
    lv_obj_align(bitrate_dd, LV_ALIGN_TOP_LEFT, 0, 50);
    lv_obj_set_style_bg_color(bitrate_dd, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(bitrate_dd, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(bitrate_dd, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bitrate_dd, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bitrate_dd, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(bitrate_dd, bitrate_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Device Information Section
    lv_obj_t* info_section = lv_obj_create(first_row);
    lv_obj_set_size(info_section, lv_pct(48), 130);
    lv_obj_set_style_bg_color(info_section, lv_color_hex(0x262626), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(info_section, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(info_section, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(info_section, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(info_section, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(info_section, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(info_section, LV_OBJ_FLAG_SCROLLABLE);

    // Info section title
    lv_obj_t* info_title = lv_label_create(info_section);
    lv_label_set_text(info_title, "DEVICE INFO");
    lv_obj_align(info_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(info_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(info_title, lv_color_hex(0x4080FF), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Serial number - adjusted positions
    lv_obj_t* serial_label = lv_label_create(info_section);
    lv_label_set_text(serial_label, "Serial Number");
    lv_obj_align(serial_label, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_obj_set_style_text_font(serial_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(serial_label, lv_color_hex(0x999999), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* serial_value = lv_label_create(info_section);
    char serial[MAX_SERIAL_LENGTH];
    if (get_device_serial(serial) == ESP_OK) {
        lv_label_set_text(serial_value, serial);
    } else {
        lv_label_set_text(serial_value, "Unknown");
    }
    lv_obj_align(serial_value, LV_ALIGN_TOP_LEFT, 0, 42);
    lv_obj_set_style_text_font(serial_value, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(serial_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Firmware version - adjusted positions
    lv_obj_t* fw_label = lv_label_create(info_section);
    lv_label_set_text(fw_label, "Firmware");
    lv_obj_align(fw_label, LV_ALIGN_TOP_LEFT, 0, 70);
    lv_obj_set_style_text_font(fw_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(fw_label, lv_color_hex(0x999999), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* fw_value = lv_label_create(info_section);
    lv_label_set_text(fw_value, FIRMWARE_VERSION);
    lv_obj_align(fw_value, LV_ALIGN_TOP_LEFT, 0, 87);
    lv_obj_set_style_text_font(fw_value, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(fw_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Second row container
    lv_obj_t* second_row = lv_obj_create(content);
    lv_obj_set_size(second_row, lv_pct(100), 200);  // Increased height to accommodate sections
    lv_obj_set_style_bg_opa(second_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(second_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(second_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(second_row, -10, LV_PART_MAIN | LV_STATE_DEFAULT);  // Reduce gap to ECU section
    lv_obj_clear_flag(second_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(second_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(second_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Network & Updates Section
    lv_obj_t* network_section = lv_obj_create(second_row);
    lv_obj_set_size(network_section, lv_pct(48), 170);  // Reduced height to fit better
    lv_obj_set_style_bg_color(network_section, lv_color_hex(0x262626), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(network_section, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(network_section, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(network_section, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(network_section, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(network_section, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(network_section, LV_OBJ_FLAG_SCROLLABLE);

    // Network section title
    lv_obj_t* network_title = lv_label_create(network_section);
    lv_label_set_text(network_title, "NETWORK & UPDATES");
    lv_obj_align(network_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(network_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(network_title, lv_color_hex(0xFF8040), LV_PART_MAIN | LV_STATE_DEFAULT);

    // WiFi button - adjusted position
    lv_obj_t* wifi_btn = lv_btn_create(network_section);
    lv_obj_set_size(wifi_btn, 180, 35);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(0x4080FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(0x5090FF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(wifi_btn, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(wifi_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t* wifi_label = lv_label_create(wifi_btn);
    lv_label_set_text(wifi_label, "Wi-Fi Settings");
    lv_obj_center(wifi_label);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(wifi_btn, wifi_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // WiFi status - adjusted position
    wifi_status_label = lv_label_create(network_section);
    lv_obj_align(wifi_status_label, LV_ALIGN_TOP_LEFT, 0, 75);
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Web server status
    web_status_label = lv_label_create(network_section);
    lv_obj_align(web_status_label, LV_ALIGN_TOP_LEFT, 0, 90);
    lv_obj_set_style_text_font(web_status_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Initial refresh of WiFi status
    refresh_wifi_status();

    // Update button - adjusted position
    lv_obj_t* update_btn = lv_btn_create(network_section);
    lv_obj_set_size(update_btn, 180, 35);
    lv_obj_align(update_btn, LV_ALIGN_TOP_LEFT, 0, 115);
    lv_obj_set_style_bg_color(update_btn, lv_color_hex(0x40FF80), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(update_btn, lv_color_hex(0x50FF90), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(update_btn, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(update_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t* update_label = lv_label_create(update_btn);
    lv_label_set_text(update_label, "Check Updates");
    lv_obj_center(update_label);
    lv_obj_set_style_text_font(update_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(update_label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(update_btn, update_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Display Settings Section
    lv_obj_t* display_section = lv_obj_create(second_row);
    lv_obj_set_size(display_section, lv_pct(48), 170);  // Reduced height to match network section
    lv_obj_set_style_bg_color(display_section, lv_color_hex(0x262626), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(display_section, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(display_section, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(display_section, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(display_section, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(display_section, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(display_section, LV_OBJ_FLAG_SCROLLABLE);

    // Display section title
    lv_obj_t* display_title = lv_label_create(display_section);
    lv_label_set_text(display_title, "DISPLAY");
    lv_obj_align(display_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(display_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(display_title, lv_color_hex(0xFFFF40), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Brightness label - adjusted position
    lv_obj_t* brightness_text = lv_label_create(display_section);
    lv_label_set_text(brightness_text, "Brightness");
    lv_obj_align(brightness_text, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_set_style_text_font(brightness_text, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(brightness_text, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Use current brightness value (don't load from NVS)
    uint8_t saved_brightness = current_brightness;

    // Brightness slider - adjusted position and size
    lv_obj_t* brightness_bar = lv_slider_create(display_section);
    lv_obj_set_size(brightness_bar, 220, 25);
    lv_obj_align(brightness_bar, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_slider_set_range(brightness_bar, 5, 100);
    lv_slider_set_value(brightness_bar, saved_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_bar, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(brightness_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(brightness_bar, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Brightness knob
    lv_obj_set_style_bg_color(brightness_bar, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(brightness_bar, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(brightness_bar, LV_RADIUS_CIRCLE, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(brightness_bar, 3, LV_PART_KNOB | LV_STATE_DEFAULT);

    // Brightness percentage label - adjusted position
    brightness_label = lv_label_create(display_section);
    lv_label_set_text_fmt(brightness_label, "Brightness: %d%%", saved_brightness);
    lv_obj_align(brightness_label, LV_ALIGN_TOP_LEFT, 0, 95);
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xFFFF40), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(brightness_bar, brightness_bar_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ECU Preconfig Section - Full width at bottom, reduced gap above
    lv_obj_t* ecu_section = lv_obj_create(content);
    lv_obj_set_size(ecu_section, lv_pct(100), 140);
    lv_obj_set_style_bg_color(ecu_section, lv_color_hex(0x262626), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ecu_section, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ecu_section, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ecu_section, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ecu_section, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ecu_section, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ecu_section, LV_OBJ_FLAG_SCROLLABLE);

    // ECU section title
    lv_obj_t* ecu_title = lv_label_create(ecu_section);
    lv_label_set_text(ecu_title, "ECU Pre-Configurations");
    lv_obj_align(ecu_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(ecu_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ecu_title, lv_color_hex(0xFF4080), LV_PART_MAIN | LV_STATE_DEFAULT);

    // ECU dropdown label
    lv_obj_t* ecu_label = lv_label_create(ecu_section);
    lv_label_set_text(ecu_label, "ECU Brand");
    lv_obj_align(ecu_label, LV_ALIGN_TOP_LEFT, 0, 35);
    lv_obj_set_style_text_font(ecu_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ecu_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Version dropdown label
    lv_obj_t* version_label = lv_label_create(ecu_section);
    lv_label_set_text(version_label, "Version/Model");
    lv_obj_align(version_label, LV_ALIGN_TOP_LEFT, 250, 35);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Status label for feedback (create first so we can pass it to callbacks)
    lv_obj_t* status_label = lv_label_create(ecu_section);
    
    // Set status text based on current ECU configuration
    char status_text[128];
    if (selected_ecu_preconfig == 0) {
        snprintf(status_text, sizeof(status_text), "[OK] Custom ECU preconfig applied");
    } else if (selected_ecu_preconfig == 1) {
        const char* version_str = (selected_ecu_version == 0) ? "1.2" : "1.3";
        snprintf(status_text, sizeof(status_text), "[OK] MaxxECU %s preconfig applied", version_str);
    } else if (selected_ecu_preconfig == 2) {
        snprintf(status_text, sizeof(status_text), "[OK] Haltech 3.10 preconfig applied");
    } else {
        snprintf(status_text, sizeof(status_text), "[OK] ECU preconfig applied");
    }
    
    lv_label_set_text(status_label, status_text);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 0, 100);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF80), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Version dropdown (create before ECU dropdown so we can pass it as user data)
    lv_obj_t* version_dd = lv_dropdown_create(ecu_section);
    lv_obj_set_size(version_dd, 200, 35);
    lv_obj_align(version_dd, LV_ALIGN_TOP_LEFT, 250, 55);
    lv_obj_set_style_bg_color(version_dd, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(version_dd, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(version_dd, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(version_dd, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(version_dd, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(version_dd, version_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, status_label);

    // Create callback data for ECU dropdown
    ecu_callback_data_t* ecu_data = lv_mem_alloc(sizeof(ecu_callback_data_t));
    ecu_data->version_dropdown = version_dd;
    ecu_data->status_label = status_label;

    // ECU dropdown
    lv_obj_t* ecu_dd = lv_dropdown_create(ecu_section);
    lv_dropdown_set_options(ecu_dd, "Custom\nMaxxECU\nHaltech");
    lv_obj_set_size(ecu_dd, 200, 35);
    lv_obj_align(ecu_dd, LV_ALIGN_TOP_LEFT, 0, 55);
    lv_obj_set_style_bg_color(ecu_dd, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ecu_dd, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ecu_dd, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ecu_dd, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ecu_dd, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_dropdown_set_selected(ecu_dd, selected_ecu_preconfig);
    lv_obj_add_event_cb(ecu_dd, ecu_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, ecu_data);

    // Initialize version dropdown based on loaded ECU preconfig
    switch (selected_ecu_preconfig) {
        case 0: // Custom
            lv_dropdown_set_options(version_dd, "Select ECU First");
            break;
        case 1: // MaxxECU
            lv_dropdown_set_options(version_dd, "1.2\n1.3");
            lv_dropdown_set_selected(version_dd, selected_ecu_version);
            break;
        case 2: // Haltech
            lv_dropdown_set_options(version_dd, "3.10");
            lv_dropdown_set_selected(version_dd, selected_ecu_version);
            break;
    }

    // Load saved bitrate setting from NVS
    nvs_handle_t handle;
    uint8_t saved_bitrate = 2; // Default to 500 kbps
    if (nvs_open("can_config", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_get_u8(handle, "can_bitrate", &saved_bitrate);
        nvs_close(handle);
    }
    lv_dropdown_set_selected(bitrate_dd, saved_bitrate);

    // Create timer to refresh WiFi status every 2 seconds
    lv_timer_create(refresh_wifi_status_timer_cb, 2000, NULL);

    lv_scr_load(settings_screen);
}

void device_settings_longpress_cb(lv_event_t* e) {
    device_settings_with_return_screen(NULL);
}