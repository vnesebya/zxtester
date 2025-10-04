#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "stdio.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "ws2812.pio.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "math.h"

// UART
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1


// OLED Configuration
#define I2C_PORT i2c1
#define I2C_SDA 6
#define I2C_SCL 7

// RGB
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


//----- OLED
void init_oled() {
    i2c_init(I2C_PORT, 100 * 1000);
    set_rgb(0, 127, 0);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

void init_i2c_safe() {
    // First, set pins to input to avoid conflicts
    gpio_init(I2C_SDA);
    gpio_init(I2C_SCL);
    gpio_set_dir(I2C_SDA, GPIO_IN);
    gpio_set_dir(I2C_SCL, GPIO_IN);
    
    // Initialize I2C with safe speed
    i2c_init(I2C_PORT, 400000);
    
    // Set pin functions
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    
    // Enable pull-ups
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Brief delay to let I2C stabilize
    sleep_ms(100);
}

void i2c_scan() {
    printf("Scanning I2C bus...\n");
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (addr % 16 == 0) printf("\n0x%02x: ", addr);
        
        uint8_t rxdata;
        int ret = i2c_read_blocking(I2C_PORT, addr, &rxdata, 1, false);
        
        if (ret == PICO_ERROR_GENERIC) {
            printf(".. ");
        } else if (ret == 1) {
            printf("%02x ", addr);
        } else {
            printf("ER ");
        }
        sleep_ms(1);
    }
    printf("\nScan complete.\n");
}

#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

void setup_uart() {
    // Инициализация UART
    uart_init(UART_ID, BAUD_RATE);
    
    // Настройка пинов
    gpio_set_function(0, GPIO_FUNC_UART); // TX
    gpio_set_function(1, GPIO_FUNC_UART); // RX
    
    // Настройка формата данных UART
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    
    // Включение FIFO
    uart_set_fifo_enabled(UART_ID, true);
}

int main() {
    stdio_init_all();
    ws2812_init();
    set_rgb(127, 0, 0);

    setup_uart();
    stdio_uart_init();

    init_i2c_safe();

//    sleep_ms(1000); // wait for usb init 

    // display test
    ssd1306_t disp;
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);
    ssd1306_fill(&disp);

    ssd1306_draw_string(&disp, 1, 1, 2, "test");
    ssd1306_show(&disp);

    set_rgb(0, 127, 0);
    printf("Started...");

    while (true) {
        printf("working...");

//        sleep_us(200);
        // set_rgb(40, 40, 40);

        // // Red
        // set_rgb(255, 0, 0);
        sleep_ms(100);
        // ssd1306_draw_string(&disp, 10, 10, 1, "1");
        
        // // Green
        // set_rgb(0, 255, 0);
        // sleep_ms(1000);
        
        // // Blue
        // set_rgb(0, 0, 255);
        // sleep_ms(1000);
        
        // // Rainbow cycle using HSV
        // for (int hue = 0; hue < 360; hue += 5) {
        //     set_hsv(hue, 100, 100);
        //     sleep_ms(50);
        //     ssd1306_draw_string(&disp, 0, 0, 1, "1");
        // }

        // color_wheel_effect();
        // fade_effect();
        // breathing_effect(32, 84, 92);
    }
}
