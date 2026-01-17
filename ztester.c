#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "math.h"
#include "stdio.h"
#include "ssd1306.h"
#include <string.h>

#include "logic_analyzer.pio.h"

#define ONBOARD_RGB

#ifdef ONBOARD_RGB
#include "ws2812.pio.h"
#endif

// UART
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

// OLED Configuration
#define OLED_I2C_PORT i2c1
#define OLED_I2C_SDA 6
#define OLED_I2C_SCL 7

// OnBoard RGB Led
#ifdef ONBOARD_RGB

#define IS_RGBW false
#define NUM_PIXELS 1
#define WS2812_PIN 16

PIO led = pio1;
int led_sm;
uint led_sm_offset;

#endif

// Logic Analyzer 
PIO la_pio = pio0;
#define SIGNAL_PIN 8
#define BTN_RIGHT_PIN 28
#define BTN_LEFT_PIN 29

#define BUFFER_SIZE 32768

uint32_t sample_buffer[BUFFER_SIZE];

volatile bool capture_complete = false;
volatile uint32_t samples_captured = 0;
volatile uint32_t words_captured = 0;
volatile uint32_t capture_count = 0;
int la_dma_channel;

void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t rgb = ((uint32_t)(r) << 8) |
                   ((uint32_t)(g) << 16) |
                   (uint32_t)(b);
    pio_sm_put_blocking(led, led_sm, rgb << 8u);
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
    led_sm_offset = pio_add_program(led, &ws2812_program);
    ws2812_program_init(led, led_sm, led_sm_offset, WS2812_PIN, 800000, IS_RGBW);
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

//----- OLEDCH340 driver
void init_oled() {
    i2c_init(OLED_I2C_PORT, 100 * 1000);
    set_rgb(0, 127, 0);

    gpio_set_function(OLED_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_I2C_SDA);
    gpio_pull_up(OLED_I2C_SCL);
}

void init_i2c_safe() {
    // First, set pins to input to avoid conflicts
    gpio_init(OLED_I2C_SDA);
    gpio_init(OLED_I2C_SCL);
    gpio_set_dir(OLED_I2C_SDA, GPIO_IN);
    gpio_set_dir(OLED_I2C_SCL, GPIO_IN);
    
    // Initialize I2C with safe speed
    i2c_init(OLED_I2C_PORT, 400000);
    
    // Set pin functions
    gpio_set_function(OLED_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_I2C_SCL, GPIO_FUNC_I2C);
    
    // Enable pull-ups
    gpio_pull_up(OLED_I2C_SDA);
    gpio_pull_up(OLED_I2C_SCL);
    
    // Brief delay to let I2C stabilize
    sleep_ms(100);
}

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    
    gpio_set_function(0, GPIO_FUNC_UART); // TX
    gpio_set_function(1, GPIO_FUNC_UART); // RX
    
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    
    uart_set_fifo_enabled(UART_ID, true);
}



// Обработчик прерывания DMA
void dma_handler() {
    if (dma_channel_get_irq0_status(la_dma_channel)) {
        dma_channel_acknowledge_irq0(la_dma_channel);
        capture_complete = true;
        words_captured = BUFFER_SIZE;
        samples_captured = BUFFER_SIZE * 32;
        dma_channel_abort(la_dma_channel);
    }
}

// returns real sampling frequency 
double setup_logic_analyzer_pio(uint sm, uint pin)  {
    uint offset = pio_add_program(la_pio, &logic_analyzer_program);
    
    pio_gpio_init(la_pio, pin);
    pio_sm_set_consecutive_pindirs(la_pio, sm, pin, 1, false);
    
    pio_sm_config c = logic_analyzer_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);
    
    const float cycles_per_sample = 2.03125f; // !
    float div = 1.0; // sampling as fast as we can 
    sm_config_set_clkdiv(&c, div);

    double achieved_sm_clock = (double)clock_get_hz(clk_sys) / (double)div;
    double achieved_sample_rate = achieved_sm_clock / (double)cycles_per_sample;
    printf("PIO clkdiv=%.6f, SM clock=%.0f Hz, sample_rate=%.2f Hz\n", div, achieved_sm_clock, achieved_sample_rate);
    
    // Shift register
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    
    pio_sm_init(la_pio, sm, offset, &c);

    // DMA init
    la_dma_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq0_enabled(la_dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    return achieved_sample_rate;
}


// Запуск захвата данных
void start_capture() {
    capture_complete = false;
    samples_captured = 0;
    words_captured = 0;
    
    // Настройка DMA
    dma_channel_config config = dma_channel_get_default_config(la_dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, pio_get_dreq(pio0, 0, false));
    
    dma_channel_configure(
        la_dma_channel,
        &config,
        sample_buffer,
        &pio0->rxf[0],
        BUFFER_SIZE,
        true
    );
    
    // Запуск PIO state machine
    pio_sm_clear_fifos(pio0, 0);
    pio_sm_restart(pio0, 0);
    pio_sm_set_enabled(pio0, 0, true);
}

// Остановка захвата
void stop_capture() {
    pio_sm_set_enabled(pio0, 0, false);
    if (!capture_complete) {
        dma_channel_abort(la_dma_channel);
    }
}

// Анализ сигнала - подсчет переходов и статистики
// capture_duration_us: duration of the capture in microseconds
void analyze_signal(const uint32_t *buffer, uint32_t word_count, uint32_t capture_id, uint32_t capture_duration_us, double sample_rate, ssd1306_t *disp) {
    uint32_t high_count = 0;
    uint32_t transitions = 0;
    uint32_t pulse_widths[2] = {0, 0}; // [0] - low pulses, [1] - high pulses
    uint32_t current_pulse_length = 0;
    uint8_t last_state = (buffer[0] & 1);
    uint8_t current_state;
    
    // Анализируем все захваченные данные
    for (uint32_t i = 0; i < word_count; i++) {
        uint32_t word = buffer[i];
        
        for (int bit = 0; bit < 32; bit++) {
            current_state = (word >> bit) & 1;
            
            if (current_state) {
                high_count++;
            }
            
            if (current_state != last_state) {
                transitions++;
                pulse_widths[last_state] += current_pulse_length;
                current_pulse_length = 1;
                last_state = current_state;
            } else {
                current_pulse_length++;
            }
        }
    }
    
    // Добавляем последний импульс
    pulse_widths[last_state] += current_pulse_length;
    
    uint32_t total_samples = word_count * 32;
    
    // Вывод результатов анализа
    printf("\n=== Capture #%lu ===\n", capture_id);
    printf("Total samples: %lu\n", total_samples);
    printf("High samples: %lu (%.1f%%)\n", high_count, (high_count * 100.0) / total_samples);
    printf("Low samples: %lu (%.1f%%)\n", total_samples - high_count, 
           ((total_samples - high_count) * 100.0) / total_samples);
    printf("Transitions: %lu\n", transitions);
    
    if (transitions > 1) {
        double capture_duration_s;
        if (sample_rate > 0.0) {
            capture_duration_s = (double)total_samples / sample_rate;
        } else {
            capture_duration_s = (double)capture_duration_us / 1e6;
        }
        double estimated_freq = (transitions / 2.0) / capture_duration_s;
        printf("Estimated frequency: %.0f Hz\n", estimated_freq);

        char s[10]={0};

        sprintf(s, "%.1f KHz", estimated_freq / 1000.0);
        ssd1306_fill(disp, 1);
        ssd1306_draw_string(disp, 1, 1, 2, s);
        ssd1306_show(disp);

        if (sample_rate > 0.0) {
            printf("(used computed capture duration %.3f ms from sample_rate %.2f Hz)\n", capture_duration_s*1000.0, sample_rate);
        } else {
            printf("(used measured capture duration %.3f ms)\n", (double)capture_duration_us/1000.0);
        }
    }
    
    if (pulse_widths[0] > 0 && pulse_widths[1] > 0) {
        printf("Average high pulse: %.2f samples\n", (float)pulse_widths[1] / (transitions / 2.0));
        printf("Average low pulse: %.2f samples\n", (float)pulse_widths[0] / (transitions / 2.0));
    }
    
    printf("First 10 words (LSB first):\n");
    for (int i = 0; i < 10 && i < word_count; i++) {
        printf("\n  Word %d: 0x%08lx - ", i, buffer[i]);
        for (int bit = 0; bit < 32; bit++) { 
            printf("%d", (buffer[i] >> bit) & 1);
        }
        //printf("");
    }
    
    if (transitions == 0) {
        printf("Signal: Constant %s\n", (high_count == total_samples) ? "HIGH" : "LOW");
    } else if (transitions == 2 && high_count == total_samples / 2) {
        printf("Signal: Perfect square wave\n");
    } else if (transitions >= 4) {
        printf("Signal: Periodic waveform\n");
    }
    
    printf("Capture duration: %.3f ms\n", (double)capture_duration_us / 1000.0);
    printf("====================\n");
}


bool detect_signal_activity(const uint32_t *buffer, uint32_t word_count) {
    // Проверяем первые 64 слова на наличие переходов
    // uint32_t check_words = (word_count < 64) ? word_count : 64;
    uint32_t check_words = word_count;
    uint8_t last_state = (buffer[0] & 1);
    
    for (uint32_t i = 0; i < check_words; i++) {
        uint32_t word = buffer[i];
        
        for (int bit = 0; bit < 32; bit++) {
            uint8_t current_state = (word >> bit) & 1;
            if (current_state != last_state) {
                return true; // Обнаружен переход - есть активность
            }
            last_state = current_state;
        }
    }
    return false; // Нет переходов - сигнал постоянный
}

int main() {
    stdio_init_all();
    set_sys_clock_hz(200000000, true);
    
    ws2812_init();
    set_rgb(127, 0, 0);

    setup_uart();
    stdio_uart_init();

    printf("Starting...");
    printf("System clock set to %lu MHz\n", (unsigned long)(clock_get_hz(clk_sys) / 1000000.));


    init_i2c_safe();
    ssd1306_t disp;
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, OLED_I2C_PORT);
    ssd1306_fill(&disp, 255);
    ssd1306_show(&disp);

    const double sample_rate = setup_logic_analyzer_pio(0, SIGNAL_PIN);
    
    printf("Configuration:\n");
    printf("  Sample pin: GPIO%d\n", SIGNAL_PIN);
    printf("  Buffer size: %d words (%d samples)\n", BUFFER_SIZE, BUFFER_SIZE * 32);
    printf("  Starting continuous capture...\n\n");
    
    uint32_t last_capture_time = time_us_32();
    bool signal_detected = false;
    uint32_t inactive_captures = 0;

    sleep_ms(200);
    while (true) {
        
        capture_count++;
        
        printf("[%lu] Starting capture... ", capture_count);
        start_capture();
        
        uint32_t capture_start = time_us_32();
        dma_channel_wait_for_finish_blocking(la_dma_channel);
        uint32_t capture_duration = time_us_32() - capture_start;
        stop_capture();

        if (!capture_complete) {
            capture_complete = true;
            words_captured = BUFFER_SIZE;
            samples_captured = BUFFER_SIZE * 32;
            dma_channel_acknowledge_irq0(la_dma_channel);
        }
        
        printf("done in %lu us, words_captured %lu\n", capture_duration, words_captured);
        bool activity = detect_signal_activity(sample_buffer, words_captured);
        
        if (activity) {
            signal_detected = true;
            inactive_captures = 0;
            printf("ACTIVE - ");
            analyze_signal(sample_buffer, words_captured, capture_count, capture_duration, sample_rate, &disp);
            set_rgb(0, 0, 127);

        } else {
            inactive_captures++;
            printf("NO SIGNAL");
            set_rgb(0, 127, 0);
            ssd1306_fill(&disp, 0);
            ssd1306_draw_string(&disp, 1, 1, 2, "No signal!");
            ssd1306_show(&disp);
            if (inactive_captures % 10 == 0) {
                printf(" (%lu consecutive no-signal captures)\n", inactive_captures);
            }
            
            if (signal_detected && inactive_captures == 1) {
                printf(">>> Signal lost after %lu active captures <<<\n", capture_count - inactive_captures);
                signal_detected = false;
            }
        }
        
        memset((void*)sample_buffer, 0, BUFFER_SIZE * sizeof(uint32_t));
        
        sleep_ms(100);
    }
    
    return 0;
}
