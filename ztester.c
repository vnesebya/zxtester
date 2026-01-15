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
#include "dutycycle.pio.h"
#include "ssd1306.h"
#include <string.h>

#include "logic_analyzer.pio.h"

#ifdef ONBOARD_RGB
#include "ws2812.pio.h"
#endif

#ifdef USING_PIO_COUNTER_PROGRAM
 #include "counter.pio.h"
#endif // DEBUG

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




#ifdef USING_PIO_COUNTER_PROGRAM

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

#endif

#define SIGNAL_PIN 8
#define BTN_RIGHT_PIN 28
#define BTN_LEFT_PIN 29

 // GPIO пин для триггера (опциона8ьно)
#define BUFFER_SIZE 16       // Размер буфера в 32-битных словах
#define SAMPLE_RATE 200000  // Желаемая частота дискретизации (200 kHz)
#define CAPTURE_INTERVAL_MS 1000 // Интервал между захватами в мс


// Глобальные переменные
uint32_t sample_buffer[BUFFER_SIZE];
volatile bool capture_complete = false;
volatile uint32_t samples_captured = 0;
volatile uint32_t words_captured = 0;
volatile uint32_t capture_count = 0;
int dma_channel;

// Обработчик прерывания DMA
void dma_handler() {
    if (dma_channel_get_irq0_status(dma_channel)) {
        dma_channel_acknowledge_irq0(dma_channel);
        capture_complete = true;
        words_captured = BUFFER_SIZE;
        samples_captured = BUFFER_SIZE * 32;
        dma_channel_abort(dma_channel);
    }
}

// Инициализация PIO для логического анализатора
PIO setup_logic_analyzer_pio(uint sm, uint pin) {
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &logic_analyzer_program);
    
    // Настройка GPIO
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    
    // Конфигурация state machine
    pio_sm_config c = logic_analyzer_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);
    
    // Расчет делителя частоты
    // The PIO loop executes 3 instructions per sample: set, in, jmp
    // Each sample executes `in pins,1` and the following `jmp` (two instructions per sample).
    const float cycles_per_sample = 2.0f;
    float div = (float)clock_get_hz(clk_sys) / (SAMPLE_RATE * cycles_per_sample);
    // Clamp divider to minimum 1.0 (can't make SM faster than system clock)
    if (div < 1.0f) div = 1.0f;
    sm_config_set_clkdiv(&c, div);
    // record achieved sample rate for later analysis
    double achieved_sm_clock = (double)clock_get_hz(clk_sys) / (double)div;
    double achieved_sample_rate = achieved_sm_clock / (double)cycles_per_sample;
    printf("PIO clkdiv=%.6f, SM clock=%.0f Hz, sample_rate=%.2f Hz\n", div, achieved_sm_clock, achieved_sample_rate);
    // store globally for analysis
    extern double g_actual_sample_rate;
    g_actual_sample_rate = achieved_sample_rate;
    
    // Настройка сдвигового регистра
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    
    pio_sm_init(pio, sm, offset, &c);
    return pio;
}

// Глобальная реальная частота сэмплирования, вычисляемая при инициализации PIO
double g_actual_sample_rate = 0.0;

// Запуск захвата данных
void start_capture() {
    capture_complete = false;
    samples_captured = 0;
    words_captured = 0;
    
    // Настройка DMA
    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, pio_get_dreq(pio0, 0, false));
    
    dma_channel_configure(
        dma_channel,
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
        dma_channel_abort(dma_channel);
    }
}

// Анализ сигнала - подсчет переходов и статистики
// capture_duration_us: duration of the capture in microseconds
void analyze_signal(const uint32_t *buffer, uint32_t word_count, uint32_t capture_id, uint32_t capture_duration_us) {
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
        if (g_actual_sample_rate > 0.0) {
            capture_duration_s = (double)total_samples / g_actual_sample_rate;
        } else {
            capture_duration_s = (double)capture_duration_us / 1e6;
        }
        double estimated_freq = (transitions / 2.0) / capture_duration_s;
        printf("Estimated frequency: %.2f Hz\n", estimated_freq);
        if (g_actual_sample_rate > 0.0) {
            printf("(used computed capture duration %.3f ms from sample_rate %.2f Hz)\n", capture_duration_s*1000.0, g_actual_sample_rate);
        } else {
            printf("(used measured capture duration %.3f ms)\n", (double)capture_duration_us/1000.0);
        }
    }
    
    if (pulse_widths[0] > 0 && pulse_widths[1] > 0) {
        printf("Average high pulse: %.2f samples\n", (float)pulse_widths[1] / (transitions / 2.0));
        printf("Average low pulse: %.2f samples\n", (float)pulse_widths[0] / (transitions / 2.0));
    }
    
    // Вывод первых нескольких слов для визуализации
    printf("First 3 words (LSB first):\n");
    for (int i = 0; i < 3 && i < word_count; i++) {
        printf("  Word %d: 0x%08lx - ", i, buffer[i]);
        for (int bit = 0; bit < 16; bit++) { // Показываем первые 16 бит
            printf("%d", (buffer[i] >> bit) & 1);
        }
        printf("...\n");
    }
    
    // Детектирование типичных сигналов
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

// Быстрый анализ для обнаружения активности
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

    printf("Started...");

    PIO pio = setup_logic_analyzer_pio(0, SIGNAL_PIN);
    
    // Инициализация DMA
    dma_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    
    printf("Configuration:\n");
    printf("  Sample pin: GPIO%d\n", SIGNAL_PIN);
    printf("  Buffer size: %d words (%d samples)\n", BUFFER_SIZE, BUFFER_SIZE * 32);
    printf("  Sample rate: %.2f MHz\n", SAMPLE_RATE / 1000000.0);
    printf("  Capture interval: %d ms\n", CAPTURE_INTERVAL_MS);
    printf("  Starting continuous capture...\n\n");
    
    uint32_t last_capture_time = time_us_32();
    bool signal_detected = false;
    uint32_t inactive_captures = 0;

     while (true) {
        uint32_t current_time = time_us_32();
        
        // Запускаем захват по истечении интервала
        if (current_time - last_capture_time >= CAPTURE_INTERVAL_MS * 1000) {
            capture_count++;
            last_capture_time = current_time;
            
            printf("[%lu] Starting capture... ", capture_count);
            start_capture();
            
            // Ожидаем завершения захвата
            uint32_t capture_start = time_us_32();
            // Use blocking DMA wait as a robust fallback if IRQ isn't firing
            dma_channel_wait_for_finish_blocking(dma_channel);
            uint32_t capture_duration = time_us_32() - capture_start;
            stop_capture();

            if (!capture_complete) {
                // If the IRQ handler didn't run for some reason, mark capture done
                capture_complete = true;
                words_captured = BUFFER_SIZE;
                samples_captured = BUFFER_SIZE * 32;
                // Acknowledge any pending IRQ to keep state consistent
                dma_channel_acknowledge_irq0(dma_channel);
            }
            
            printf("done in %lu us, words_captured %lu", capture_duration, words_captured);
            
            // Анализируем сигнал
            bool activity = detect_signal_activity(sample_buffer, words_captured);
            
            if (activity) {
                signal_detected = true;
                inactive_captures = 0;
                printf("ACTIVE - ");
                // Детальный анализ для активных сигналов
                analyze_signal(sample_buffer, words_captured, capture_count, capture_duration);
            } else {
                inactive_captures++;
                printf("NO SIGNAL");
                
                // Периодически выводим статистику даже для отсутствия сигнала
                if (inactive_captures % 10 == 0) {
                    printf(" (%lu consecutive no-signal captures)", inactive_captures);
                }
                printf("\n");
                
                // Специальный вывод при первом обнаружении отсутствия сигнала после активности
                if (signal_detected && inactive_captures == 1) {
                    printf(">>> Signal lost after %lu active captures <<<\n", capture_count - inactive_captures);
                    signal_detected = false;
                }
            }
            
            // Очистка буфера для следующего захвата
            memset((void*)sample_buffer, 0, BUFFER_SIZE * sizeof(uint32_t));
            
            // Небольшая пауза перед следующим захватом
            sleep_ms(10);
        }
        
        // Проверка на прерывание по пользовательскому вводу (опционально)
        // if (stdio_usb_connected()) {
        //     int c = getchar_timeout_us(0);
        //     if (c == 'q' || c == 'Q') {
        //         printf("\n\nStopping capture...\n");
        //         stop_capture();
        //         break;
        //     } else if (c == 's' || c == 'S') {
        //         printf("\n\nForce signal analysis:\n");
        //         analyze_signal(sample_buffer, words_captured, capture_count);
        //     }
        // }
        
        sleep_ms(1);
    }
    
    return 0;

}
