#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_intr_alloc.h"
#include "esp_pm.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_touch_gt911.h"
#include "lvgl.h"
#include "driver/twai.h"
#include "ui_Screen1.h"
#include "driver/i2c.h"
#include "ui/ui.h"
#include "lvgl_helpers.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include <unistd.h>       // For unlink
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include "screens/ui_Screen3.h"
#include "esp32s3/rom/cache.h"
#include "driver/ledc.h"

#define EXAMPLE_MAX_CHAR_SIZE    64
#define MOUNT_POINT "/sdcard"
sdmmc_card_t *card;  // Declare globally if not done already

// Define the LVGL mutex
SemaphoreHandle_t lvgl_mux = NULL;

#define LV_USE_ANIMATION 0
#define LV_USE_SHADOW 0
#define LV_USE_BLEND_MODES 0


#define I2C_MASTER_SCL_IO           9       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           8       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0       /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define GPIO_INPUT_IO_4    4
#define GPIO_INPUT_PIN_SEL  1
static const char *TAG = "main";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (18 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_BK_LIGHT       -1
#define EXAMPLE_PIN_NUM_HSYNC          46
#define EXAMPLE_PIN_NUM_VSYNC          3
#define EXAMPLE_PIN_NUM_DE             5
#define EXAMPLE_PIN_NUM_PCLK           7
#define EXAMPLE_PIN_NUM_DATA0          14 // B3
#define EXAMPLE_PIN_NUM_DATA1          38 // B4
#define EXAMPLE_PIN_NUM_DATA2          18 // B5
#define EXAMPLE_PIN_NUM_DATA3          17 // B6
#define EXAMPLE_PIN_NUM_DATA4          10 // B7
#define EXAMPLE_PIN_NUM_DATA5          39 // G2
#define EXAMPLE_PIN_NUM_DATA6          0 // G3
#define EXAMPLE_PIN_NUM_DATA7          45 // G4
#define EXAMPLE_PIN_NUM_DATA8          48 // G5
#define EXAMPLE_PIN_NUM_DATA9          47 // G6
#define EXAMPLE_PIN_NUM_DATA10         21 // G7
#define EXAMPLE_PIN_NUM_DATA11         1  // R3
#define EXAMPLE_PIN_NUM_DATA12         2  // R4
#define EXAMPLE_PIN_NUM_DATA13         42 // R5
#define EXAMPLE_PIN_NUM_DATA14         41 // R6
#define EXAMPLE_PIN_NUM_DATA15         40 // R7
#define EXAMPLE_PIN_NUM_DISP_EN        -1
#define LCD_CMD_BITS_DEFAULT          8
#define LCD_PARAM_BITS_DEFAULT        8
#define LCD_RGB_PANEL_WRITE_BYTES     NULL  // Use default write bytes function

// The pixel number in horizontal and vertical
#define EXAMPLE_LCD_H_RES              800
#define EXAMPLE_LCD_V_RES              480

#if CONFIG_EXAMPLE_DOUBLE_FB
#define EXAMPLE_LCD_NUM_FB             2
#else
#define EXAMPLE_LCD_NUM_FB             1
#endif // CONFIG_EXAMPLE_DOUBLE_FB

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 30
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 20
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (8 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     1

// we use two semaphores to sync the VSYNC event and the LVGL task, to avoid potential tearing effect
#if CONFIG_EXAMPLE_AVOID_TEAR_EFFECT_WITH_SEM
SemaphoreHandle_t sem_vsync_end;
SemaphoreHandle_t sem_gui_ready;
#endif

// Declare the flush callback function
void my_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

lv_disp_drv_t disp_drv;
lv_disp_draw_buf_t draw_buf;

// Configuration for the CAN bus - make them global
twai_timing_config_t g_t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(20,19, TWAI_MODE_NORMAL);
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };


    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

static bool example_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
#if CONFIG_EXAMPLE_AVOID_TEAR_EFFECT_WITH_SEM
    if (xSemaphoreTakeFromISR(sem_gui_ready, &high_task_awoken) == pdTRUE) {
        xSemaphoreGiveFromISR(sem_vsync_end, &high_task_awoken);
    }
#endif
    return high_task_awoken == pdTRUE;
}

#define MOUNT_POINT "/sdcard"
#define SD_MOSI     11
#define SD_CLK      12
#define SD_MISO     13
#define SD_CS       4  // GPIO for CS

void init_sd_card(void) {
    esp_err_t ret;
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI("SD_CARD", "Initializing SD card");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Configure the SPI bus
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 10000;  // Reduced frequency for compatibility

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI,
        .miso_io_num = SD_MISO,
        .sclk_io_num = SD_CLK,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE("SD_CARD", "Failed to initialize bus.");
        return;
    }

    // Configure the SD card slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS;
    slot_config.host_id = host.slot;

    // Mount the filesystem
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE("SD_CARD", "Failed to mount filesystem. Error: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI("SD_CARD", "SD card mounted successfully");
    sdmmc_card_print_info(stdout, card);
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // Ensure the flush area is aligned to cache line size (32 bytes)
    size_t size = (offsetx2 - offsetx1 + 1) * (offsety2 - offsety1 + 1) * sizeof(lv_color_t);
    if (size > 0) {
        // Ensure the color_map buffer is aligned
        if (((intptr_t)color_map & 0x1F) == 0) {
            // Buffer is already aligned
            Cache_WriteBack_Addr((uint32_t)color_map, size);
        } else {
            // Buffer needs alignment handling
            static DRAM_ATTR lv_color_t *aligned_buf = NULL;
            static size_t aligned_size = 0;
            
            if (aligned_size < size) {
                if (aligned_buf) {
                    free(aligned_buf);
                }
                aligned_size = (size + 31) & ~31;  // Round up to 32 bytes
                aligned_buf = heap_caps_aligned_alloc(32, aligned_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                assert(aligned_buf != NULL);
            }
            
            memcpy(aligned_buf, color_map, size);
            Cache_WriteBack_Addr((uint32_t)aligned_buf, size);
            color_map = aligned_buf;
        }
    }

#if CONFIG_EXAMPLE_AVOID_TEAR_EFFECT_WITH_SEM
    xSemaphoreGive(sem_gui_ready);
    xSemaphoreTake(sem_vsync_end, portMAX_DELAY);
#endif

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

bool example_lvgl_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If timeout_ms is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void example_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

static void example_lvgl_port_task(void *arg) {
    ESP_LOGI(TAG, "Starting LVGL task with 6ms refresh rate");
    const uint32_t refresh_period_ms = 30;  // Refresh interval in milliseconds

    while (1) {
        // Lock the mutex as LVGL APIs are not thread-safe
        if (example_lvgl_lock(-1)) {
            lv_timer_handler();  // Handle LVGL timers and refresh the screen
            example_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(refresh_period_ms));  // Fixed delay for consistent refresh
    }
}

void gpio_init(void)
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //bit mask of the pins, use GPIO6 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    //enable pull-up mode
     io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
}

// extern lv_obj_t *scr;
static void example_lvgl_touch_cb(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read touch controller data */
    esp_lcd_touch_read_data(drv->user_data);

    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(drv->user_data, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void can_init() {
    // Install the CAN driver
    if (twai_driver_install(&g_config, &g_t_config, &f_config) == ESP_OK) {
        ESP_LOGI(TAG, "CAN driver installed successfully");
    } else {
        ESP_LOGE(TAG, "Failed to install CAN driver");
    }

    // Start the CAN driver
    if (twai_start() == ESP_OK) {
        ESP_LOGI(TAG, "CAN driver started");
    } else {
        ESP_LOGE(TAG, "Failed to start CAN driver");
    }
}

void can_receive_task(void *pvParameter) {
    while (1) {
        twai_message_t message;
        // Use 12 ms delay between processing CAN messages
        if (twai_receive(&message, portMAX_DELAY) == ESP_OK) {
            // Process the received CAN message
            process_can_message(&message); // Implement this function as needed
        } else {
            ESP_LOGE(TAG, "Failed to receive CAN message");
        }
        vTaskDelay(0 / portTICK_PERIOD_MS); // Add a delay of 1 millisecond to save CPU
    }
}

void test_sd_card_write() {
    const char *file_path = MOUNT_POINT"/no.txt";
    FILE *file = fopen(file_path, "w");  // Open file for writing
    if (file == NULL) {
        ESP_LOGE("SD_CARD", "Failed to open file for writing");
        return;
    }
    
    // Write "Hello, World!" to the file
    fprintf(file, "you suck balls\n");
    fclose(file);  // Close the file
    ESP_LOGI("SD_CARD", "File written successfully: %s", file_path);
}

extern warning_config_t warning_configs[8];
extern value_config_t values_config[13];  // from your ui code
extern char label_texts[13][64];
extern char value_offset_texts[13][64];
extern int rpm_gauge_max;
#define RPM_VALUE_ID 9
#define SPEED_VALUE_ID 10
#define GEAR_VALUE_ID 11
#define BAR1_VALUE_ID 12
#define BAR2_VALUE_ID 13

void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
}

void load_values_config_from_nvs(void) {
    nvs_handle_t handle;
    if (nvs_open("can_config", NVS_READWRITE, &handle) == ESP_OK) {
        for (int i = 0; i < 13; i++) {
            char key[32];
            uint8_t temp_u8;
            float temp_f;
            uint32_t temp_u32;

            // enabled
            snprintf(key, sizeof(key), "enabled%d", i);
            if (nvs_get_u8(handle, key, &temp_u8) == ESP_OK) {
                values_config[i].enabled = (bool)temp_u8;
            }

            // can_id
            snprintf(key, sizeof(key), "can_id%d", i);
            if (nvs_get_u32(handle, key, &temp_u32) == ESP_OK) {
                values_config[i].can_id = temp_u32;
            }

            // endianess
            snprintf(key, sizeof(key), "endian%d", i);
            if (nvs_get_u8(handle, key, &temp_u8) == ESP_OK) {
                values_config[i].endianess = temp_u8;
            }

            // bit_start
            snprintf(key, sizeof(key), "bit_st%d", i);
            if (nvs_get_u8(handle, key, &temp_u8) == ESP_OK) {
                values_config[i].bit_start = temp_u8;
            }

            // bit_length
            snprintf(key, sizeof(key), "bit_len%d", i);
            if (nvs_get_u8(handle, key, &temp_u8) == ESP_OK) {
                values_config[i].bit_length = temp_u8;
            }

            // decimals
            snprintf(key, sizeof(key), "decimals%d", i);
            if (nvs_get_u8(handle, key, &temp_u8) == ESP_OK) {
                values_config[i].decimals = temp_u8;
            }

            // value_offset
            size_t len_f = sizeof(temp_f);
            snprintf(key, sizeof(key), "val_off%d", i);
            if (nvs_get_blob(handle, key, &temp_f, &len_f) == ESP_OK) {
                values_config[i].value_offset = temp_f;
            }

            // scale
            len_f = sizeof(temp_f);
            snprintf(key, sizeof(key), "scale%d", i);
            if (nvs_get_blob(handle, key, &temp_f, &len_f) == ESP_OK) {
                values_config[i].scale = temp_f;
            }

            // is_signed
            snprintf(key, sizeof(key), "is_signed%d", i);
            uint8_t signed_val;
            if (nvs_get_u8(handle, key, &signed_val) == ESP_OK) {
                values_config[i].is_signed = (signed_val == 1);
            }

            // Load RPM bar color for RPM value
            if (i == RPM_VALUE_ID - 1) {
                uint32_t color_value;
                snprintf(key, sizeof(key), "rpm_color%d", i);
                if (nvs_get_u32(handle, key, &color_value) == ESP_OK) {
                    values_config[i].rpm_bar_color.full = color_value;
                }
            }

            // Load warning settings for panels (indices 0-7)
            if (i < 8) {
                float temp_warn_f;
                uint32_t temp_color;
                uint8_t warn_flags;
                size_t len_warn = sizeof(float);

                // Warning High Threshold
                snprintf(key, sizeof(key), "warn_hi_th%d", i);
                if (nvs_get_blob(handle, key, &temp_warn_f, &len_warn) == ESP_OK) {
                    values_config[i].warning_high_threshold = temp_warn_f;
                }

                // Warning Low Threshold
                snprintf(key, sizeof(key), "warn_lo_th%d", i);
                if (nvs_get_blob(handle, key, &temp_warn_f, &len_warn) == ESP_OK) {
                    values_config[i].warning_low_threshold = temp_warn_f;
                }

                // Warning High Color
                snprintf(key, sizeof(key), "warn_hi_col%d", i);
                if (nvs_get_u32(handle, key, &temp_color) == ESP_OK) {
                    values_config[i].warning_high_color.full = temp_color;
                }

                // Warning Low Color
                snprintf(key, sizeof(key), "warn_lo_col%d", i);
                if (nvs_get_u32(handle, key, &temp_color) == ESP_OK) {
                    values_config[i].warning_low_color.full = temp_color;
                }

                // Warning Enabled Flags
                snprintf(key, sizeof(key), "warn_enabled%d", i);
                if (nvs_get_u8(handle, key, &warn_flags) == ESP_OK) {
                    values_config[i].warning_high_enabled = (warn_flags & 0x02) != 0;
                    values_config[i].warning_low_enabled = (warn_flags & 0x01) != 0;
                }
            }

            // Load bar min/max for bar values
            if (i == BAR1_VALUE_ID - 1 || i == BAR2_VALUE_ID - 1) {
                int32_t temp_val;
                
                snprintf(key, sizeof(key), "bar_min%d", i);
                if (nvs_get_i32(handle, key, &temp_val) == ESP_OK) {
                    values_config[i].bar_min = temp_val;
                }
                
                snprintf(key, sizeof(key), "bar_max%d", i);
                if (nvs_get_i32(handle, key, &temp_val) == ESP_OK) {
                    values_config[i].bar_max = temp_val;
                }
            }
        }

        // Load labels
        for (int i = 0; i < 13; i++) {
            char key[32];
            size_t required_size = sizeof(label_texts[i]);
            snprintf(key, sizeof(key), "label%d", i);
            nvs_get_str(handle, key, label_texts[i], &required_size);
        }

        // Load RPM gauge max
        int32_t temp_rpm;
        if (nvs_get_i32(handle, "rpm_max", &temp_rpm) == ESP_OK) {
            rpm_gauge_max = temp_rpm;
        }

        nvs_close(handle);
    }
}

void save_values_config_to_nvs(void) {
    nvs_handle_t handle;
    if (nvs_open("can_config", NVS_READWRITE, &handle) == ESP_OK) {
        for (int i = 0; i < 13; i++) {
            char key[32];

            // enabled
            snprintf(key, sizeof(key), "enabled%d", i);
            uint8_t en = values_config[i].enabled ? 1 : 0;
            nvs_set_u8(handle, key, en);
            vTaskDelay(pdMS_TO_TICKS(1));

            // can_id
            snprintf(key, sizeof(key), "can_id%d", i);
            nvs_set_u32(handle, key, values_config[i].can_id);
            vTaskDelay(pdMS_TO_TICKS(1));

            // endianess
            snprintf(key, sizeof(key), "endian%d", i);
            uint8_t end = (values_config[i].endianess == 0) ? 0 : 1;
            nvs_set_u8(handle, key, end);
            vTaskDelay(pdMS_TO_TICKS(1));

            // bit_start
            snprintf(key, sizeof(key), "bit_st%d", i);
            nvs_set_u8(handle, key, values_config[i].bit_start);
            vTaskDelay(pdMS_TO_TICKS(1));

            // bit_length
            snprintf(key, sizeof(key), "bit_len%d", i);
            nvs_set_u8(handle, key, values_config[i].bit_length);
            vTaskDelay(pdMS_TO_TICKS(1));

            // decimals
            snprintf(key, sizeof(key), "decimals%d", i);
            nvs_set_u8(handle, key, values_config[i].decimals);
            vTaskDelay(pdMS_TO_TICKS(1));

            // value_offset
            snprintf(key, sizeof(key), "val_off%d", i);
            nvs_set_blob(handle, key, &values_config[i].value_offset, sizeof(values_config[i].value_offset));
            vTaskDelay(pdMS_TO_TICKS(1));

            // scale
            snprintf(key, sizeof(key), "scale%d", i);
            nvs_set_blob(handle, key, &values_config[i].scale, sizeof(values_config[i].scale));
            vTaskDelay(pdMS_TO_TICKS(1));

            // is_signed
            snprintf(key, sizeof(key), "is_signed%d", i);
            uint8_t signed_val = values_config[i].is_signed ? 1 : 0;
            nvs_set_u8(handle, key, signed_val);
            vTaskDelay(pdMS_TO_TICKS(1));

            // Save RPM bar color for RPM value
            if (i == RPM_VALUE_ID - 1) {
                snprintf(key, sizeof(key), "rpm_color%d", i);
                uint32_t color_value = values_config[i].rpm_bar_color.full;
                nvs_set_u32(handle, key, color_value);
                vTaskDelay(pdMS_TO_TICKS(1));
            }

            // Save warning settings for panels (indices 0-7)
            if (i < 8) {
                // Warning High Threshold
                snprintf(key, sizeof(key), "warn_hi_th%d", i);
                nvs_set_blob(handle, key, &values_config[i].warning_high_threshold, 
                           sizeof(values_config[i].warning_high_threshold));
                vTaskDelay(pdMS_TO_TICKS(1));

                // Warning Low Threshold
                snprintf(key, sizeof(key), "warn_lo_th%d", i);
                nvs_set_blob(handle, key, &values_config[i].warning_low_threshold,
                           sizeof(values_config[i].warning_low_threshold));
                vTaskDelay(pdMS_TO_TICKS(1));

                // Warning High Color
                snprintf(key, sizeof(key), "warn_hi_col%d", i);
                uint32_t hi_color = values_config[i].warning_high_color.full;
                nvs_set_u32(handle, key, hi_color);
                vTaskDelay(pdMS_TO_TICKS(1));

                // Warning Low Color
                snprintf(key, sizeof(key), "warn_lo_col%d", i);
                uint32_t lo_color = values_config[i].warning_low_color.full;
                nvs_set_u32(handle, key, lo_color);
                vTaskDelay(pdMS_TO_TICKS(1));

                // Warning Enabled Flags
                snprintf(key, sizeof(key), "warn_enabled%d", i);
                uint8_t warn_flags = (values_config[i].warning_high_enabled ? 0x02 : 0) |
                                   (values_config[i].warning_low_enabled ? 0x01 : 0);
                nvs_set_u8(handle, key, warn_flags);
                vTaskDelay(pdMS_TO_TICKS(1));
            }

            // Save bar min/max for bar values
            if (i == BAR1_VALUE_ID - 1 || i == BAR2_VALUE_ID - 1) {
                snprintf(key, sizeof(key), "bar_min%d", i);
                nvs_set_i32(handle, key, values_config[i].bar_min);
                vTaskDelay(pdMS_TO_TICKS(1));
                
                snprintf(key, sizeof(key), "bar_max%d", i);
                nvs_set_i32(handle, key, values_config[i].bar_max);
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }

        // Save labels
        for (int i = 0; i < 13; i++) {
            char key[32];
            snprintf(key, sizeof(key), "label%d", i);
            nvs_set_str(handle, key, label_texts[i]);
            vTaskDelay(pdMS_TO_TICKS(2));
        }

        // Save RPM gauge max
        nvs_set_i32(handle, "rpm_max", rpm_gauge_max);
        vTaskDelay(pdMS_TO_TICKS(2));

        nvs_commit(handle);
        nvs_close(handle);
    }
}

void save_warning_configs_to_nvs(void) {
    nvs_handle_t handle;
    if (nvs_open("warn_config", NVS_READWRITE, &handle) == ESP_OK) {
        for (int i = 0; i < 8; i++) {
            char key[32];
            
            // Save CAN ID
            snprintf(key, sizeof(key), "warn_can_id%d", i);
            esp_err_t err = nvs_set_u32(handle, key, warning_configs[i].can_id);
            if (err != ESP_OK) {
                printf("Error saving CAN ID for warning %d: %s\n", i, esp_err_to_name(err));
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            
            // Save bit position
            snprintf(key, sizeof(key), "warn_bit_pos%d", i);
            err = nvs_set_u8(handle, key, warning_configs[i].bit_position);
            if (err != ESP_OK) {
                printf("Error saving bit position for warning %d: %s\n", i, esp_err_to_name(err));
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            
            // Save active color
            snprintf(key, sizeof(key), "warn_color%d", i);
            uint32_t color_value = warning_configs[i].active_color.full;
            err = nvs_set_u32(handle, key, color_value);
            if (err != ESP_OK) {
                printf("Error saving color for warning %d: %s\n", i, esp_err_to_name(err));
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            
            // Save label
            snprintf(key, sizeof(key), "warn_label%d", i);
            err = nvs_set_str(handle, key, warning_configs[i].label);
            if (err != ESP_OK) {
                printf("Error saving label for warning %d: %s\n", i, esp_err_to_name(err));
            }
            vTaskDelay(pdMS_TO_TICKS(1));

            // Save toggle mode setting
            snprintf(key, sizeof(key), "warn_is_mom%d", i);
            uint8_t is_momentary = warning_configs[i].is_momentary ? 1 : 0;
            err = nvs_set_u8(handle, key, is_momentary);
            if (err != ESP_OK) {
                printf("Error saving toggle mode for warning %d: %s\n", i, esp_err_to_name(err));
            }
            vTaskDelay(pdMS_TO_TICKS(1));

            // Save current state
            snprintf(key, sizeof(key), "warn_state%d", i);
            uint8_t current_state = warning_configs[i].current_state ? 1 : 0;
            err = nvs_set_u8(handle, key, current_state);
            if (err != ESP_OK) {
                printf("Error saving current state for warning %d: %s\n", i, esp_err_to_name(err));
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        esp_err_t err = nvs_commit(handle);
        if (err != ESP_OK) {
            printf("Error committing warning configs to NVS: %s\n", esp_err_to_name(err));
        }
        nvs_close(handle);
    }
}

void load_warning_configs_from_nvs(void) {
    nvs_handle_t handle;
    if (nvs_open("warn_config", NVS_READWRITE, &handle) == ESP_OK) {
        for (int i = 0; i < 8; i++) {
            char key[32];
            
            // Load CAN ID
            snprintf(key, sizeof(key), "warn_can_id%d", i);
            uint32_t can_id;
            if (nvs_get_u32(handle, key, &can_id) == ESP_OK) {
                warning_configs[i].can_id = can_id;
                printf("Loaded warning %d CAN ID: 0x%X\n", i, can_id);
            }
            
            // Load bit position
            snprintf(key, sizeof(key), "warn_bit_pos%d", i);
            uint8_t bit_pos;
            if (nvs_get_u8(handle, key, &bit_pos) == ESP_OK) {
                warning_configs[i].bit_position = bit_pos;
                printf("Loaded warning %d bit position: %d\n", i, bit_pos);
            }
            
            // Load active color
            snprintf(key, sizeof(key), "warn_color%d", i);
            uint32_t color_value;
            if (nvs_get_u32(handle, key, &color_value) == ESP_OK) {
                warning_configs[i].active_color.full = color_value;
                printf("Loaded warning %d color: 0x%X\n", i, color_value);
            }
            
            // Load label
            snprintf(key, sizeof(key), "warn_label%d", i);
            size_t required_size = sizeof(warning_configs[i].label);
            if (nvs_get_str(handle, key, warning_configs[i].label, &required_size) == ESP_OK) {
                printf("Loaded warning %d label: %s\n", i, warning_configs[i].label);
            }

            // Load toggle mode setting
            snprintf(key, sizeof(key), "warn_is_mom%d", i);
            uint8_t is_momentary;
            if (nvs_get_u8(handle, key, &is_momentary) == ESP_OK) {
                warning_configs[i].is_momentary = (is_momentary == 1);
                printf("Loaded warning %d toggle mode: %s\n", i, 
                    warning_configs[i].is_momentary ? "Momentary" : "Toggle");
            } else {
                warning_configs[i].is_momentary = true; // Default to momentary if not found
            }

            // Load current state
            snprintf(key, sizeof(key), "warn_state%d", i);
            uint8_t current_state;
            if (nvs_get_u8(handle, key, &current_state) == ESP_OK) {
                warning_configs[i].current_state = (current_state == 1);
                printf("Loaded warning %d current state: %d\n", i, current_state);
            } else {
                warning_configs[i].current_state = false; // Default to off if not found
            }
        }
        nvs_close(handle);
    }
}

void erase_all_nvs_settings(void) {
    esp_err_t err;

    // Erase the entire NVS partition
    err = nvs_flash_erase();
    if (err == ESP_OK) {
        printf("NVS erased successfully.\n");

        // After erasing, reinitialize NVS to use it later in the app
        err = nvs_flash_init();
        if (err == ESP_OK) {
            printf("NVS reinitialized successfully.\n");
        } else {
            printf("Error reinitializing NVS: %s\n", esp_err_to_name(err));
        }
    } else {
        printf("Error erasing NVS: %s\n", esp_err_to_name(err));
    }
}

void app_main(void)
{
	init_nvs();
	
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions


ESP_LOGI(TAG, "Install RGB LCD panel driver");
esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_rgb_panel_config_t panel_config = {
    .data_width = 16,                            // RGB565 in parallel mode, thus 16bit in width
    .psram_trans_align = 64,
    .num_fbs = EXAMPLE_LCD_NUM_FB,               // Number of frame buffers
#if CONFIG_EXAMPLE_USE_BOUNCE_BUFFER
    .bounce_buffer_size_px = 10 * EXAMPLE_LCD_H_RES,
#endif
    .clk_src = LCD_CLK_SRC_DEFAULT,
    .disp_gpio_num = EXAMPLE_PIN_NUM_DISP_EN,
    .pclk_gpio_num = EXAMPLE_PIN_NUM_PCLK,
    .vsync_gpio_num = EXAMPLE_PIN_NUM_VSYNC,
    .hsync_gpio_num = EXAMPLE_PIN_NUM_HSYNC,
    .de_gpio_num = EXAMPLE_PIN_NUM_DE,
    .data_gpio_nums = {
        EXAMPLE_PIN_NUM_DATA0,
        EXAMPLE_PIN_NUM_DATA1,
        EXAMPLE_PIN_NUM_DATA2,
        EXAMPLE_PIN_NUM_DATA3,
        EXAMPLE_PIN_NUM_DATA4,
        EXAMPLE_PIN_NUM_DATA5,
        EXAMPLE_PIN_NUM_DATA6,
        EXAMPLE_PIN_NUM_DATA7,
        EXAMPLE_PIN_NUM_DATA8,
        EXAMPLE_PIN_NUM_DATA9,
        EXAMPLE_PIN_NUM_DATA10,
        EXAMPLE_PIN_NUM_DATA11,
        EXAMPLE_PIN_NUM_DATA12,
        EXAMPLE_PIN_NUM_DATA13,
        EXAMPLE_PIN_NUM_DATA14,
        EXAMPLE_PIN_NUM_DATA15,
    },
    .timings = {
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .h_res = EXAMPLE_LCD_H_RES,
        .v_res = EXAMPLE_LCD_V_RES,
        .hsync_back_porch = 8,
        .hsync_front_porch = 8,
        .hsync_pulse_width = 4,
        .vsync_back_porch = 16,
        .vsync_front_porch = 16,
        .vsync_pulse_width = 4,
        .flags.pclk_active_neg = true,
    },
    .flags = {
        .fb_in_psram = true,                     // Allocate frame buffer in PSRAM
        .no_fb = false,                          // Use frame buffer
        .refresh_on_demand = false,              // Continuous refresh
#if CONFIG_EXAMPLE_USE_BOUNCE_BUFFER
        .bb_invalidate_cache = true,             // Invalidate cache when using bounce buffer
#endif
    }
};

ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    ESP_LOGI(TAG, "Register event callbacks");
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = example_on_vsync_event,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, &disp_drv));

    ESP_LOGI(TAG, "Initialize RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
    
#endif
gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    // Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");
   // gpio_init();
    // Set initial configuration for I2C device at 0x24
    uint8_t write_buf = 0x01;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    // Additional configuration for SD card CS pin at address 0x38
    write_buf = 0x0A;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    esp_rom_delay_us(100 * 1000);


    // Reset the touch screen as part of the initial setup
    write_buf = 0x2C;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    esp_rom_delay_us(100 * 1000);

    gpio_set_level(GPIO_INPUT_IO_4, 0); // Set GPIO level for reset
    esp_rom_delay_us(100 * 1000);

    // Continue with touch screen initialization
    write_buf = 0x2E;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    esp_rom_delay_us(200 * 1000);

    esp_lcd_touch_handle_t tp = NULL;
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;

    ESP_LOGI(TAG, "Initialize I2C");

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

    ESP_LOGI(TAG, "Initialize touch IO (I2C)");
    /* Touch IO handle */
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_MASTER_NUM, &tp_io_config, &tp_io_handle));
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_V_RES,
        .y_max = EXAMPLE_LCD_H_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    /* Initialize touch */
    ESP_LOGI(TAG, "Initialize touch controller GT911");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

ESP_LOGI(TAG, "Allocate separate LVGL draw buffers from PSRAM");
// Align buffer size to 32 bytes
size_t buf_size = ((EXAMPLE_LCD_H_RES * 100 * sizeof(lv_color_t) + 31) & ~31);
void *buf1 = heap_caps_aligned_alloc(32, buf_size, MALLOC_CAP_SPIRAM);
void *buf2 = heap_caps_aligned_alloc(32, buf_size, MALLOC_CAP_SPIRAM);
assert(buf1 && buf2);
lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 100);
	

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
#if CONFIG_EXAMPLE_DOUBLE_FB
    disp_drv.full_refresh = false; // the full_refresh mode can maintain the synchronization between the two frame buffers
#endif
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;    // Input device driver (Touch)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    indev_drv.user_data = tp;

    lv_indev_drv_register(&indev_drv);

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL, 1);

    ESP_LOGI(TAG, "Display LVGL Scatter Chart");
    
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (example_lvgl_lock(-1)) {
        ui_init();  // Initialize your LVGL UI

        example_lvgl_unlock();
    }

    // Initialize CAN bus
    //init_sd_card();
    can_init();
   // test_sd_card_write();

    xTaskCreatePinnedToCore(can_receive_task, "can_receive_task", 4096, NULL, 7, NULL, 0);  // Pin to core 0
}
