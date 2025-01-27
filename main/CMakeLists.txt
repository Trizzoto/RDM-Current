cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_options("-Wno-format")
project(racedash)

set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_SOURCE_DIR}/components/espressif__esp_io_expander-v1.0.1"
    "${CMAKE_SOURCE_DIR}/components/sd_card"
)