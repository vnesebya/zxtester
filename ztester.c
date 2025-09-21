#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "font.h"



#define IS_RGBW false
#define NUM_PIXELS 1
#define WS2812_PIN 16

PIO pio = pio0;
int sm = 0;


// Function to set RGB color (0-255 for each channel)
void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t rgb = ((uint32_t)(r) << 8) |
                   ((uint32_t)(g) << 16) |
                   (uint32_t)(b);
    pio_sm_put_blocking(pio, sm, rgb << 8u);
}

// Function to set HSV color (hue: 0-359, saturation: 0-100, value: 0-100)
void set_hsv(uint16_t hue, uint8_t sat, uint8_t val) {
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
    
    set_rgb((r + m) * 255, (g + m) * 255, (b + m) * 255);
}

// Initialize the WS2812 LED
void ws2812_init() {
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
}

// Fade effect
void fade_effect() {
    for (int i = 0; i < 256; i++) {
        set_rgb(i, 0, 0);  // Fade in red
        sleep_ms(5);
    }
    for (int i = 255; i >= 0; i--) {
        set_rgb(i, 0, 0);  // Fade out red
        sleep_ms(5);
    }
}

// Breathing effect
void breathing_effect(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < 100; i++) {
        float factor = (sin(i * 0.1) + 1) / 2.0f;
        set_rgb(r * factor, g * factor, b * factor);
        sleep_ms(30);
    }
}

// Color wheel effect
void color_wheel_effect() {
    for (int i = 0; i < 360; i++) {
        set_hsv(i, 100, 100);
        sleep_ms(20);
    }
}

int main() {
    stdio_init_all();
    ws2812_init();
    
 
    
    printf("OLED Display Test\n");


    while (true) {
        // Red
        set_rgb(255, 0, 0);
        sleep_ms(1000);
        
        // Green
        set_rgb(0, 255, 0);
        sleep_ms(1000);
        
        // Blue
        set_rgb(0, 0, 255);
        sleep_ms(1000);
        
        // Rainbow cycle using HSV
        for (int hue = 0; hue < 360; hue += 5) {
            set_hsv(hue, 100, 100);
            sleep_ms(50);
        }

	color_wheel_effect();
	fade_effect();
	breathing_effect(32, 84, 92);
    }
}
