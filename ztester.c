#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <pico/multicore.h>
#include <hardware/dma.h>
#include <hardware/uart.h>
#include <hardware/i2c.h>
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ws2812.h"
#include "ssd1306.h"
#include "sampler.pio.h"
#include "sampler.h"

// Debug UART
#define DBG_UART_ID uart0
#define DBG_UART_BAUDRATE 115200
#define DBG_UART_TX_PIN 0
#define DBG_UART_RX_PIN 1
#define DBG_UART_DATA_BITS 8
#define DBG_UART_STOP_BITS 1
#define DBG_PARITY UART_PARITY_NONE

// OnBoard RGB Led
ws2812_t ws2812 = {
    .pio = pio1,
    .sm = 0,
    .offset = 0,
    .pin = 16,
    .rgbw = false
};

// OLED Configuration
ssd1306_t oled = {
    .i2c_port = i2c1,
    .width = 128,
    .height = 64,
    .address = 0x3C,
    .external_vcc = false,
    .SCL = 7,
    .SDA = 6
};

#define BTN_RIGHT_PIN 28
#define BTN_LEFT_PIN 29

// Logic Analyzer 
#define SIGNAL_PIN 8
#define BUFFER_SIZE 32768

uint32_t sampler_buffer[BUFFER_SIZE];
sampler_t sampler = {
    .pio = pio0,
    .pin = SIGNAL_PIN,
    .sample_buffer = sampler_buffer,
    .buffer_size = BUFFER_SIZE
};

void setup_uart(uart_inst_t *uart, uint baudrate, uint tx, uint rx, uint databits, uint stopbits, uart_parity_t parity) {
    uart_init(uart, baudrate);
    
    gpio_set_function(tx, GPIO_FUNC_UART); // TX
    gpio_set_function(rx, GPIO_FUNC_UART); // RX
    
    uart_set_format(uart, databits, stopbits, parity);
    
    uart_set_fifo_enabled(uart, true);
}

// Анализ сигнала - подсчет переходов и статистики
void analyze_signal(const uint32_t *buffer, uint32_t word_count, uint32_t capture_id, double sample_rate) {
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
        double capture_duration_s = (double)total_samples / sample_rate;
        double estimated_freq = (transitions / 2.0) / capture_duration_s;
        printf("Estimated frequency: %.0f Hz\n", estimated_freq);

        char s[10]={0};

        sprintf(s, "%.1f KHz", estimated_freq / 1000.0);
        ssd1306_fill(&oled, 0);
        ssd1306_draw_string(&oled, 1, 1, 2, s);
        ssd1306_show(&oled);

        printf("(used computed capture duration %.3f ms from sample_rate %.2f Hz)\n", capture_duration_s*1000.0, sample_rate);
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
    
    ws2812_init(&ws2812);
    set_rgb(127, 0, 0, &ws2812);

    setup_uart(DBG_UART_ID, DBG_UART_BAUDRATE, DBG_UART_TX_PIN, DBG_UART_RX_PIN, DBG_UART_DATA_BITS, DBG_UART_STOP_BITS, DBG_PARITY);
    stdio_uart_init();

    printf("Starting...");
    printf("System clock set to %lu MHz\n", (unsigned long)(clock_get_hz(clk_sys) / 1000000.));

    ssd1306_init(&oled);
    ssd1306_fill(&oled, 255);
    ssd1306_show(&oled);

    const double sample_rate = setup_sampler(&sampler);
    
    printf("Configuration:\n");
    printf("  Sample pin: GPIO%d\n", SIGNAL_PIN);
    printf("  Sample rate: %.1f\n", sample_rate);
    printf("  Buffer size: %d words (%d samples)\n", BUFFER_SIZE, BUFFER_SIZE * 32);
    printf("  Starting continuous capture...\n\n");
    sleep_ms(200);
    
    bool signal_detected = false;
    uint32_t inactive_captures = 0;
    uint32_t capture_count = 0;
    while (true) {
        
        capture_count++;
        
        printf("[%lu] Starting capture... ", capture_count);

        start_capture(&sampler);
        wait_capture_blocking(&sampler);
        stop_capture(&sampler);
         
        bool activity = detect_signal_activity(sampler.sample_buffer, BUFFER_SIZE);
        
        if (activity) {
            signal_detected = true;
            inactive_captures = 0;
            printf("ACTIVE - ");
            analyze_signal(sampler.sample_buffer, BUFFER_SIZE, capture_count, sample_rate);
            set_rgb(0, 0, 127, &ws2812);

        } else {
            inactive_captures++;
            printf("NO SIGNAL");
            set_rgb(0, 96, 0, &ws2812);
            ssd1306_fill(&oled, 0);
            ssd1306_draw_string(&oled, 1, 1, 2, "No signal!");
            ssd1306_show(&oled);
            if (inactive_captures % 10 == 0) {
                printf(" (%lu consecutive no-signal captures)\n", inactive_captures);
            }
            
            if (signal_detected && inactive_captures == 1) {
                printf(">>> Signal lost after %lu active captures <<<\n", capture_count - inactive_captures);
                signal_detected = false;
            }
        }
        //sleep_ms(100);
    }
    
    return 0;
}
