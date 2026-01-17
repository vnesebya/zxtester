#include <stdint.h>
#include "hardware/pio.h"

typedef struct {
    PIO pio;
    int sm;
    uint offset;
    int pin;
    bool rgbw;
} ws2812_t;


// Initialize the WS2812 LED
void ws2812_init(ws2812_t* ws2812);

void set_rgb(uint8_t r, uint8_t g, uint8_t b, ws2812_t* ws2812);
void set_hsv(uint16_t hue, uint8_t sat, uint8_t val, ws2812_t* ws2812);

void fade_effect(ws2812_t* ws2812);
void breathing_effect(uint8_t r, uint8_t g, uint8_t b, ws2812_t* ws2812);
void color_wheel_effect(ws2812_t* ws2812);
