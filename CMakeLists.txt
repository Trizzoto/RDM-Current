cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "esp32-firmware")

# Set compiler path
set(CMAKE_C_COMPILER "C:/Users/ruuva/esp/v5.3.1/tools/xtensa-esp32s3-elf/esp-2023r1-12.2.0/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-gcc.exe")

# Include the ESP-IDF project cmake file
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Add compile options
add_compile_options("-Wno-format")

# Set extra component directories before the project() command
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_SOURCE_DIR}/components/espressif__esp_io_expander-v1.0.1"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/sd_card"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/lvgl__lvgl"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/espressif__esp_lcd_touch"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/espressif__esp_lcd_touch_gt911"
)

# Project declaration
project(${PROJECT_NAME})
