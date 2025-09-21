cmake_minimum_required(VERSION 3.12)

#include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(pico-sdk/external/pico_sdk_import.cmake)

set(TARGET ztester)

project(${TARGET})

pico_sdk_init()

add_executable(${TARGET}
    ztester.c
    ssd1306.c
)

# Add the WS2812 PIO program
pico_generate_pio_header(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_link_libraries(${TARGET} 
	pico_stdlib 
	pico_multicore 
	hardware_pio
	hardware_gpio
	hardware_i2c
)

pico_add_extra_outputs(${TARGET})
