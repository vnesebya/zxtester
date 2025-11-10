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

#pico_enable_stdio_usb(${TARGET} 1)
pico_enable_stdio_uart(${TARGET} 1)

# Add the WS2812 PIO program
#pico_generate_pio_header(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
#pico_generate_pio_header(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/counter.pio)
pico_generate_pio_header(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/dutycycle.pio)

target_link_libraries(${TARGET} 
	pico_stdlib 
#	pico_multicore 
	hardware_pio
	hardware_gpio
	hardware_i2c
#	hardware_pwm
#	pico_stdio_usb
)

pico_add_extra_outputs(${TARGET})
