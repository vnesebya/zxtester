#include "ws2812.h"

#include "math.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "ws2812.pio.h"

// Initialize the WS2812 LED
void ws2812_init(ws2812_t* ws2812) {
    ws2812->offset = pio_add_program(ws2812->pio, &ws2812_program);
    ws2812_program_init(ws2812->pio, ws2812->sm, ws2812->offset, ws2812->pin, 800000, ws2812->rgbw);
}

// Fade effect
void fade_effect(ws2812_t *ws2812) {
    for (int i = 0; i < 256; i++) {
        set_rgb(i, 0, 0, ws2812);
        sleep_ms(5);
    }
    for (int i = 255; i >= 0; i--) {
        set_rgb(i, 0, 0, ws2812);
        sleep_ms(5);
    }
}

// Breathing effect
void breathing_effect(uint8_t r, uint8_t g, uint8_t b, ws2812_t *ws2812) {
    for (int i = 0; i < 100; i++) {
        float factor = (sin(i * 0.1) + 1) / 2.0f;
        set_rgb(r * factor, g * factor, b * factor, ws2812);
        sleep_ms(30);
    }
}

// Color wheel effect
void color_wheel_effect(ws2812_t *ws2812) {
    for (int i = 0; i < 360; i++) {
        set_hsv(i, 100, 100, ws2812);
        sleep_ms(20);
    }
}

void set_rgb(uint8_t r, uint8_t g, uint8_t b, ws2812_t* ws2812) {
    uint32_t rgb = ((uint32_t)(r) << 8) |
                   ((uint32_t)(g) << 16) |
                   (uint32_t)(b);
    pio_sm_put_blocking(ws2812->pio, ws2812->sm, rgb << 8u);
}

// Function to set HSV color (hue: 0-359, saturation: 0-100, value: 0-100)
void set_hsv(uint16_t hue, uint8_t sat, uint8_t val, ws2812_t* ws2812) {
    hue = hue % 360;
    float s = sat / 100.0f;
    float v = val / 100.0f;
    
    float c = v * s;
    float x = c * (1 - fabs(fmod(hue / 60.0f, 2) - 1));
    float m = v - c;
    
    float r, g, b;
    
    if (hue < 60) {
        r = c; g = x; b = 0;
    } else if (hue < 120) {
        r = x; g = c; b = 0;
    } else if (hue < 180) {
        r = 0; g = c; b = x;
    } else if (hue < 240) {
        r = 0; g = x; b = c;
    } else if (hue < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }
    
    set_rgb((r + m) * 255, (g + m) * 255, (b + m) * 255, ws2812);
}
