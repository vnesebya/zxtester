#include <hardware/uart.h>
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <stdio.h>
#include <string.h>

#include "ws2812.h"
#include "ssd1306.h"
#include "sampler.h"
#include "analyzer.h"

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

// analyze_signal_buffer moved to analyzer.c; types are declared in analyzer.h

// Print analysis_result_t and update OLED as before
void print_analysis_result(const analysis_result_t * res, uint32_t capture_id) {
    printf("\n=== Capture #%lu ===\n", capture_id);
    printf("Total samples: %lu\n", (unsigned long)res->total_samples);
    printf("High samples: %lu (%.1f%%)\n", (unsigned long)res->high_count,
           (res->high_count * 100.0) / res->total_samples);
    printf("Low samples: %lu (%.1f%%)\n", (unsigned long)(res->total_samples - res->high_count),
           ((res->total_samples - res->high_count) * 100.0) / res->total_samples);
    printf("Transitions: %lu\n", (unsigned long)res->transitions);

    if (res->transitions > 1) {
        printf("Estimated frequency: %.0f Hz\n", res->estimated_freq);

        char s[16] = {0};
        sprintf(s, "%.1f KHz", res->estimated_freq / 1000.0);
        ssd1306_fill(&oled, 0);
        ssd1306_draw_string(&oled, 1, 1, 2, s);
        ssd1306_show(&oled);

        printf("(used computed capture duration %.3f ms from sample_rate %.2f Hz)\n", res->capture_duration_s * 1000.0, (double)(res->total_samples) / res->capture_duration_s);
    }

    if (res->pulse_widths[0] > 0 && res->pulse_widths[1] > 0 && res->transitions > 1) {
        printf("Average high pulse: %.2f samples\n", res->avg_high_pulse);
        printf("Average low pulse: %.2f samples\n", res->avg_low_pulse);
    }

    printf("First 10 words (LSB first):\n");
    for (int i = 0; i < 10 && i < (int)res->word_count; i++) {
        printf("\n  Word %d: 0x%08lx - ", i, (unsigned long)res->first_words[i]);
        for (int bit = 0; bit < 32; bit++) {
            printf("%d", (res->first_words[i] >> bit) & 1);
        }
    }
    printf("\n");

    if (res->signal_type == SIGNAL_TYPE_CONSTANT) {
        printf("Signal: Constant %s\n", (res->high_count == res->total_samples) ? "HIGH" : "LOW");
    } else if (res->signal_type == SIGNAL_TYPE_PERFECT_SQUARE) {
        printf("Signal: Perfect square wave\n");
    } else if (res->signal_type == SIGNAL_TYPE_PERIODIC) {
        printf("Signal: Periodic waveform\n");
    }

    printf("====================\n");
}


bool detect_signal_activity(const uint32_t *buffer, uint32_t word_count) {
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
            printf("ACTIVE");
            analysis_result_t analysis = analyze_signal_buffer(sampler.sample_buffer, BUFFER_SIZE, sample_rate);
            print_analysis_result(&analysis, capture_count);
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
