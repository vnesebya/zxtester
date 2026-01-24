#include <hardware/uart.h>
#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <stdio.h>
#include <string.h>

#include "ws2812.h"
#include "ssd1306.h"
#include "sampler.h"
#include "analyzer.h"
#include "units.h"
#include "button.h"

// Buttons
#define BTN_RIGHT_PIN 14
#define BTN_LEFT_PIN 13

// Display samples
#define MIN_DISPLAY_SAMPLES 2
#define MAX_DISPLAY_SAMPLES 32
#define DISPLAY_SAMPLES 8

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
    .i2c_port = i2c0,
    .width = 128,
    .height = 64,
    .address = 0x3C,
    .external_vcc = false,
    .SDA = 4,
    .SCL = 5,
};


// Signal sampler
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
    
    gpio_set_function(tx, GPIO_FUNC_UART);
    gpio_set_function(rx, GPIO_FUNC_UART);
    
    uart_set_format(uart, databits, stopbits, parity);
    
    uart_set_fifo_enabled(uart, true);
}

void print_analysis_result(const analysis_result_t * res, uint32_t capture_id, const uint32_t *buffer, double sample_rate, uint32_t display_samples) {
    printf("\n=== Capture #%lu ===\n", capture_id);
    printf("Total samples: %lu\n", (unsigned long)res->total_samples);
    printf("High samples: %lu (%.1f%%)\n", (unsigned long)res->high_count,
           (res->high_count * 100.0) / res->total_samples);

    printf("Avg low length: %.1f, Avg high length: %.1f\n", res->avg_low_pulse, res->avg_high_pulse);

    printf("Low samples: %lu (%.1f%%)\n", (unsigned long)(res->total_samples - res->high_count),
           ((res->total_samples - res->high_count) * 100.0) / res->total_samples);
    printf("Transitions: %lu\n", (unsigned long)res->transitions);

    ssd1306_fill(&oled, 0);
    if (res->transitions > 1) {
        printf("Estimated frequency: %.0f Hz\n", res->estimated_freq);

        char s[16] = {0};
        char d[16] = {0};
        // sprintf(s, "%.3f KHz", res->estimated_freq / 1000.0);
        printFreq (s, res->estimated_freq);

        sprintf(d, "Duty %.1f%%", res->duty_cycle);
        ssd1306_draw_string(&oled, 1, 1, s);
        ssd1306_draw_string(&oled, 1, 24, d);

        printf("(used computed capture duration %.3f ms from sample_rate %.2f Hz)\n", res->capture_duration_s * 1000.0, (double)(res->total_samples) / res->capture_duration_s);
    }

    if (res->pulse_widths[0] > 0 && res->pulse_widths[1] > 0 && res->transitions > 1) {
        printf("Average high pulse: %.2f samples\n", res->pulse_widths);
        printf("Average low pulse: %.2f samples\n", res->avg_low_pulse);
    }

    printf("Duty cycle: %.1f%%\n", res->duty_cycle);

    // Reduced 32-bit pattern (remove spikes) and display
    reduce_t reduced[128] = {0};


    reduce_buffer_to_32(buffer, res->word_count, reduced, res->high_count / (res->transitions * 2));

    printf("reduced:\n");
    for (int i = 0; i < 32; ++i) {
        if (reduced[i] == 0) {
            printf("0");
        } else {
            printf("1");
        }
    }
    printf("\n");



    // char bits[33] = {0};
    // uint32_t display_samples = 16;
    uint32_t sample_width = oled.width / display_samples;

    uint16_t zero_y = 62;
    uint16_t one_y = 46;
    uint16_t sample_height = zero_y - one_y;
    reduce_t last_val = reduced[0];
    uint16_t cursor = 0;

    uint16_t zero_width = (1.0 - (res->duty_cycle / 100.0)) * sample_width * 4.0;
    uint16_t one_width = (res->duty_cycle / 100.0) * sample_width * 4.0;
    // uint16_t zero_width = (1.0 - (res->duty_cycle / 100.0)) * sample_width;
    // uint16_t one_width = (res->duty_cycle / 100.0) * sample_width;

    for (uint32_t i = 0; i < display_samples; i++) {

        if (last_val != reduced[i]) {
            for (int n = one_y; n < zero_y; ++n)
                ssd_draw_fullpixel(&oled, cursor, n, true, 1);
        }
        
        switch (reduced[i]) {
            case reduced_one: {
                for (int n = 0; n < one_width; ++n)
                    ssd_draw_fullpixel(&oled, cursor + n, one_y, true, 1);
                cursor += one_width;    
                last_val = reduced[i];
            } break;
            
            case reduced_zero: {
                for (int n = 0; n < zero_width; ++n)
                    ssd_draw_fullpixel(&oled, cursor + n, zero_y, true, 1);
                cursor += zero_width;    
                last_val = reduced[i];
            } break;

            case reduced_pin: {
                for (int n = one_y; n < zero_y; ++n)
                    ssd_draw_fullpixel(&oled, cursor, n, true, 1);
                cursor += 1;    
            } break;
        }
        if (cursor >= 127 ) 
            break;
    }

    putchar('\n');
    // ssd1306_draw_string(&oled, 1, 40, bits);
    ssd1306_show(&oled);

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

int main() {
    stdio_init_all();
    set_sys_clock_hz(128000000, true);

    Button btn1;
    button_init(&btn1, BTN_LEFT_PIN);  // кнопка на GPIO2
    Button btn2;
    button_init(&btn2, BTN_RIGHT_PIN);  // кнопка на GPIO2
    
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
    uint32_t min_display_samples = MIN_DISPLAY_SAMPLES;
    uint32_t max_display_samples = MAX_DISPLAY_SAMPLES;
    uint32_t display_samples = DISPLAY_SAMPLES;
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

            print_analysis_result(&analysis, capture_count, sampler.sample_buffer, sample_rate, display_samples);
            set_rgb(0, 0, 127, &ws2812);


        } else {
            inactive_captures++;
            printf("NO SIGNAL");
            set_rgb(45, 45, 0, &ws2812);
            ssd1306_fill(&oled, 0);
            ssd1306_draw_string(&oled, 1, 1, "No signal!");
            ssd1306_show(&oled);
            if (inactive_captures % 10 == 0) {
                printf(" (%lu consecutive no-signal captures)\n", inactive_captures);
            }
            
            if (signal_detected && inactive_captures == 1) {
                printf(">>> Signal lost after %lu active captures <<<\n", capture_count - inactive_captures);
                signal_detected = false;
            }
        }

        // Left button
        button_tick(&btn1);
        if (button_click(&btn1)) {
            if (display_samples < max_display_samples) display_samples++;
        }
        if (button_hold(&btn1)) {
            if (display_samples < max_display_samples) display_samples = display_samples * 2;
        }
        // Right button
        button_tick(&btn2);
        if (button_click(&btn2)) {
            if (display_samples > min_display_samples) display_samples--;
        }
        if (button_hold(&btn2)) {
            if (display_samples > min_display_samples) display_samples = display_samples / 2;
        }

        // sleep_ms(3000);
    }
    
    return 0;
}
