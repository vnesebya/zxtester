#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#define SIGNAL_PIN 2
#define SAMPLE_TIME_MS 1000

// Global variables for measurement
volatile uint32_t rising_edge_time = 0;
volatile uint32_t falling_edge_time = 0;
volatile uint32_t period = 0;
volatile uint32_t high_time = 0;
volatile uint32_t low_time = 0;
volatile bool measurement_ready = false;

// GPIO interrupt handler
void gpio_callback(uint gpio, uint32_t events) {
    uint32_t current_time = time_us_32();
    
    if (events & GPIO_IRQ_EDGE_RISE) {
        if (rising_edge_time != 0) {
            period = current_time - rising_edge_time;
        }
        rising_edge_time = current_time;
        
        if (falling_edge_time > rising_edge_time) {
            high_time = falling_edge_time - rising_edge_time;
        }
    }
    else if (events & GPIO_IRQ_EDGE_FALL) {
        falling_edge_time = current_time;
        
        if (rising_edge_time > 0 && falling_edge_time > rising_edge_time) {
            low_time = falling_edge_time - rising_edge_time;
            measurement_ready = true;
        }
    }
}

void initialize_gpio() {
    gpio_init(SIGNAL_PIN);
    gpio_set_dir(SIGNAL_PIN, GPIO_IN);
    gpio_pull_up(SIGNAL_PIN);  // Optional: add pull-up if needed
    
    // Enable interrupts on both edges
    gpio_set_irq_enabled_with_callback(SIGNAL_PIN, 
                                      GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, 
                                      true, 
                                      &gpio_callback);
}

void calculate_and_print_duty() {
    if (period > 0 && high_time > 0 && low_time > 0) {
        float duty_plus = (float)high_time / period * 100.0f;
        float duty_minus = (float)low_time / period * 100.0f;
        float frequency = 1000000.0f / period;  // Convert to Hz
        
        printf("Period: %lu us, High: %lu us, Low: %lu us\n", 
               period, high_time, low_time);
        printf("Duty+: %.2f%%, Duty-: %.2f%%, Frequency: %.2f Hz\n", 
               duty_plus, duty_minus, frequency);
        printf("---\n");
    }
}

int main() {
    stdio_init_all();
    
    printf("RP2040 Duty Cycle Measurement\n");
    printf("Measuring signal on GPIO %d\n", SIGNAL_PIN);
    printf("Sampling time: %d ms\n\n", SAMPLE_TIME_MS);
    
    initialize_gpio();
    
    absolute_time_t last_print_time = get_absolute_time();
    
    while (true) {
        if (measurement_ready) {
            calculate_and_print_duty();
            measurement_ready = false;
            
            // Reset measurements periodically to handle signal changes
            last_print_time = get_absolute_time();
        }
        
        // Check if we should reset measurements (in case signal stops)
        if (absolute_time_diff_us(last_print_time, get_absolute_time()) > SAMPLE_TIME_MS * 1000) {
            rising_edge_time = 0;
            falling_edge_time = 0;
            period = 0;
            high_time = 0;
            low_time = 0;
            last_print_time = get_absolute_time();
        }
        
        sleep_ms(10);
    }
    
    return 0;
}