#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "math.h"
#include "stdio.h"

// PWM

// Конфигурация сигнала
//#define PWM_FREQUENCY 56000000  // 56 МГц
//#define PWM_FREQUENCY 56000000  // 
#define TARGET_FREQ 18750000    // 56 МГц
#define DUTY_CYCLE 50           // 50% скважность

#define PWM_FREQUENCY 400000  // 
#define DEFAULT_DUTY_CYCLE 50   // Скважность по умолчанию 50%

// Пины (можно изменить)
#define PWM_PIN 14

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

// bool setup_precise_pwm(uint pin, uint32_t freq, uint8_t duty_cycle) {
//     gpio_set_function(pin, GPIO_FUNC_PWM);
    
//     float divider = (float)clock_get_hz(clk_sys) / (2.0f * TARGET_FREQ);
    
//     printf("Расчетный делитель: %.6f\n", divider);

//     uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
//     uint channel = pwm_gpio_to_channel(PWM_PIN);
    
//     // Устанавливаем делитель частоты
//     pwm_set_clkdiv(slice_num, divider);
    
//     // Устанавливаем период (TOP = 1 для максимальной частоты)
//     pwm_set_wrap(slice_num, 1);
    
//     // Устанавливаем скважность 50%
//     // При TOP = 1, уровень = 1 дает 50% скважность
//     pwm_set_chan_level(slice_num, channel, 1);
    
//     // Включаем PWM
//     pwm_set_enabled(slice_num, true);
    
//     return false;
// }

bool setup_precise_pwm(uint pin, uint32_t freq, uint8_t duty_cycle) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
    
    uint32_t sys_clk = clock_get_hz(clk_sys);
    
    // Пробуем разные значения TOP для лучшей точности
    for (uint16_t top = 1; top <= 10; top++) {
        double divider = (double)sys_clk / (freq * (top + 1));
        
        if (divider >= 1.0f / 256.0f && divider <= 255.0f) {
            pwm_set_clkdiv(slice_num, divider);
            pwm_set_wrap(slice_num, top);
            
            // Расчет уровня для заданной скважности
            uint16_t level = (duty_cycle * (top + 1)) / 100;
            pwm_set_chan_level(slice_num, channel, level);
            
            pwm_set_enabled(slice_num, true);
            
            double actual_freq = (double)sys_clk / (divider * (top + 1));
            printf("TOP=%d, Делитель=%.6f, Факт.частота=%.2f МГц\n", 
                   top, divider, actual_freq / 1000000.0);
            
            return true;
        }
    }
    
    return false;
}

int main() {
    stdio_init_all();
    ws2812_init();
    init_i2c_safe();

    sleep_ms(1000);
    //set_rgb(127, 127, 127);

    //gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    
    // uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    // uint channel = pwm_gpio_to_channel(PWM_PIN);
    
    float clock_hz = (float)clock_get_hz(clk_sys);
    // float divider = clock_hz / (PWM_FREQUENCY * 256.0);
    
//     if (divider < 1.0) {
//         set_rgb(127, 0, 0);
//     } else {
// // //        set_rgb(0, 0, 127);
// //         // Если требуемая частота слишком высокая
// //         // printf("Ошибка: требуемая частота слишком высока!\n");
// //         // printf("Максимальная частота PWM: %.2f МГц\n", (clock_hz / 256.0) / 1000000.0);
// //         pwm_set_clkdiv(slice_num, divider);
        
// //         // Устанавливаем период PWM
// //         // TOP значение определяет период: период = (TOP + 1) * (делитель / системная_частота)
// //         pwm_set_wrap(slice_num, 255); // 8-битное разрешение
        
// //         // Устанавливаем скважность по умолчанию
// //         pwm_set_chan_level(slice_num, channel, 128); // 50% при TOP=255
        
// //         // Включаем PWM
// //         pwm_set_enabled(slice_num, true);

//         //max speed
//     }

// #ifdef FALS
//     uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
//     uint channel = pwm_gpio_to_channel(PWM_PIN);
//     pwm_set_clkdiv(slice_num, 1.0f);
//     // Минимальный период (TOP = 1)
//     pwm_set_wrap(slice_num, 1);
//     // Скважность 50% (1/2)
//     pwm_set_chan_level(slice_num, channel, 1);
//     pwm_set_enabled(slice_num, true);
//     set_rgb(0, 0, 127);
// #else
    if (setup_precise_pwm(PWM_PIN, TARGET_FREQ/4, DUTY_CYCLE)) {
        set_rgb(127, 0, 0);

    } else {
        set_rgb(0, 127, 0);
    }
// #endif

    // display test
    ssd1306_t disp;
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);
    ssd1306_fill(&disp);

    ssd1306_draw_string(&disp, 1, 1, 2, "test");
    ssd1306_show(&disp);


    //set_rgb(0, 127, 0);

    while (true) {
        // printf("Максимальная частота PWM: %.2f МГц\n", (clock_hz / 256.0) / 1000000.0);
        // printf("sysclk: %.2f МГц\n", clock_hz / 1000000.0);
        // printf("Частота: %d Гц (%.2f МГц)\n", PWM_FREQUENCY, PWM_FREQUENCY / 1000000.0);
        // printf("Скважность: %d%%\n", DEFAULT_DUTY_CYCLE);
        // printf("Системная частота: %.2f МГц\n", clock_hz / 1000000.0);
        // // printf("Делитель: %.6f\n", divider);
        // sleep_ms(1000);

        

        sleep_us(200);

        // set_rgb(40, 40, 40);
        //ssd1306_clear(&disp);

        // // Red
        // set_rgb(255, 0, 0);
        // sleep_ms(1000);
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
