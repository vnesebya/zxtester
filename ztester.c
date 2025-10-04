#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "pio_squarewave.pio.h" // Автоматически сгенерированный файл

#define OUTPUT_PIN 14

int main() {
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &squarewave_program);

//    set_sys_clock_khz(112000, true);

    //    float desired_freq = 3.5e6 * 16; // 56 МГц
//    float desired_freq = 3.5e6;
//    float desired_freq = 14e6;
///    float desired_freq = 56e6;
    float desired_freq = 31.25e6;
    float div = (float)clock_get_hz(clk_sys) / (2 * desired_freq); // 125,000,000 / (2 * 56,000,000) ≈ 1.116

    squarewave_program_init(pio, sm, offset, OUTPUT_PIN, div);
    save_and_disable_interrupts();

    while (true) {
        tight_loop_contents();
    }
    return 0;
}