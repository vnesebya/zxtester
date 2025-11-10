#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "math.h"
#include "stdio.h"

#include "ssd1306.h"

#ifdef ONBOARD_RGB
#include "ws2812.pio.h"
#endif

#include "counter.pio.h"

// UART
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE


// OLED Configuration
#define I2C_PORT i2c1
#define I2C_SDA 6
#define I2C_SCL 7

// RGB
#ifdef ONBOARD_RGB
#define IS_RGBW false
#define NUM_PIXELS 1
#define WS2812_PIN 16
#endif

// PIO
PIO pio = pio0;
int sm = 0;
uint offset = 0;

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

#ifdef ONBOARD_RGB
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
#endif

//----- OLEDCH340 driver
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


void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    
    gpio_set_function(0, GPIO_FUNC_UART); // TX
    gpio_set_function(1, GPIO_FUNC_UART); // RX
    
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    
    uart_set_fifo_enabled(UART_ID, true);
}



volatile uint32_t pulse_count = 0;

// Обработчик прерывания PIO для инкремента счетчика
void pio_irq_handler() {
    pio_interrupt_clear(pio, 0); // Очищаем прерывание 0
    pulse_count++; // Увеличиваем счетчик
}

void counter_init(uint pin) {
    // Загружаем PIO программу
    offset = pio_add_program(pio, &counter_program);
    
    // Конфигурируем State Machine
    pio_sm_config config = counter_program_get_default_config(offset);
    
    // Настраиваем GPIO пин
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    sm_config_set_in_pins(&config, pin);
    sm_config_set_jmp_pin(&config, pin);
    gpio_pull_up(pin);

    
    // Настраиваем сдвиговые регистры
    // sm_config_set_in_shift(&config, false, false, 32);
    // sm_config_set_out_shift(&config, false, false, 32);
    sm_config_set_in_shift(&config, true, true, 32);
    sm_config_set_out_shift(&config, true, true, 32);
    
    // Настраиваем FIFO
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    
    // Настраиваем clock divider (максимальная скорость)
    sm_config_set_clkdiv(&config, 1.0f);
    
    // Инициализируем State Machine
    pio_sm_init(pio, sm, offset, &config);
    
    // Настраиваем начальное значение OSR
    pio_sm_put(pio, sm, 0); // Начальное значение счетчика = 0
    
    // Запускаем State Machine
    pio_sm_set_enabled(pio, sm, true);
    
    printf("PIO Counter initialized on pin %d\n", pin);
}

// Функция чтения счетчика из PIO
uint32_t pio_counter_read2() {
    // Останавливаем SM для чтения
    pio_sm_set_enabled(pio, sm, false);
    
    // Сохраняем текущее состояние
    pio_sm_exec(pio, sm, pio_encode_push(false, true));
    
    // Читаем значение
    uint32_t count = pio_sm_get(pio, sm);
    
    // Перезапускаем SM
    pio_sm_restart(pio, sm);
    pio_sm_set_enabled(pio, sm, true);
    
    return count;
}

uint32_t pio_counter_read() {
    static uint32_t last_value = 0;
    
    while (!pio_sm_is_rx_fifo_empty(pio, sm)) {
        last_value = last_value + pio_sm_get(pio, sm);
    }
    return last_value;
}

uint32_t pio_counter_read3() {
    static uint32_t last_value = 0;
    if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
        last_value = pio_sm_get(pio, sm);
    }
    return last_value;
}

// Альтернативный метод с использованием прерываний PIO
void counter_init_with_irq(uint pin) {
    offset = pio_add_program(pio, &counter_program);
    
    pio_sm_config config = counter_program_get_default_config(offset);
    
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    sm_config_set_in_pins(&config, pin);
    
    // Настройка сдвиговых регистров
    sm_config_set_in_shift(&config, false, false, 32);
    sm_config_set_out_shift(&config, false, false, 32);
    
    // Настройка clock divider
    sm_config_set_clkdiv(&config, 1.0f);
    
    // Настройка прерываний
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
    
    pio_sm_init(pio, sm, offset, &config);
    
    // Инициализируем счетчик
    pio_sm_put(pio, sm, 0);
    pio_sm_set_enabled(pio, sm, true);
    
    printf("PIO Counter with IRQ initialized on pin %d\n", pin);
}

volatile uint32_t last_print = 0;
volatile uint32_t total_pulses = 0;


int main() {
    stdio_init_all();

    // pio_sm_config c = pio_get_default_sm_config();
    // sm_config_set_in_shift(&c, true, true, 32);
    // sm_config_set_clkdiv(&c, 1.0f);
    // bool setclkres = set_sys_clock_khz(133000, true);
    
    //ws2812_init();
    //set_rgb(127, 0, 0);

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

    // set_rgb(0, 127, 0);

    printf("Started... %1");

    const uint COUNT_PIN = 2; 
    
    counter_init(COUNT_PIN);
    
    uint32_t last_display_time = time_us_32();
    uint32_t last_count = 0;

    while (true) {
        uint32_t current_count = 0xffffffff - pio_counter_read2();
        //uint32_t current_count = pio_sm_get_blocking(pio, sm);
        //uint32_t current_count = pulse_count;
        
        uint32_t current_time = time_us_32();
        if (current_time - last_display_time >= 2000000) {
            uint32_t delta = current_count - last_count;
            float frequency = (float)delta / ((current_time - last_display_time) / 1000000.0f);
            
            printf("Total pulses: %u, Frequency: %.1f Hz\n", 
                   current_count, frequency);
            
            last_count = current_count;
            last_display_time = current_time;
        }
        //printf("long string abcdefghijklmnopqrstuvwxyz");
        sleep_ms(200);
    }
}
