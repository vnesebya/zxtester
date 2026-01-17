cmake_minimum_required(VERSION 3.12)

#include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(../pico-sdk/external/pico_sdk_import.cmake)

set(TARGET ztester)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

project(${TARGET} C CXX ASM)

pico_sdk_init()

add_executable(${TARGET}
    ztester.c
    ssd1306.c
	ws2812.c
)

#pico_enable_stdio_usb(${TARGET} 1)
pico_enable_stdio_uart(${TARGET} 1)

# Add PIO programs
pico_generate_pio_header(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/sampler.pio)
pico_generate_pio_header(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

#define PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK 1
target_compile_definitions(${TARGET} PRIVATE PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1)
target_link_libraries(${TARGET} 
	pico_stdlib 
	# pico_multicore 
	hardware_pio
	hardware_gpio
	hardware_i2c
	hardware_dma
#	hardware_pwm
#	pico_stdio_usb
)

pico_add_extra_outputs(${TARGET})
