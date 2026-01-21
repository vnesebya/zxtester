#include "sampler.h"

#include "sampler.pio.h"
#include <string.h>

int dma_channel;
volatile bool capture_complete = false;

void dma_handler() {
    if (dma_channel_get_irq0_status(dma_channel)) {
        dma_channel_acknowledge_irq0(dma_channel);
        capture_complete = true;
        dma_channel_abort(dma_channel);
    }
}

// returns real sampling frequency 
double setup_sampler(sampler_t *sampler)  {
    uint sm = 0;
    uint offset = pio_add_program(sampler->pio, &sampler_program);
    
    gpio_set_function(sampler->pin, GPIO_FUNC_NULL);

    pio_gpio_init(sampler->pio, sampler->pin);
    pio_sm_set_consecutive_pindirs(sampler->pio, sm, sampler->pin, 1, false);
    
    pio_sm_config c = sampler_program_get_default_config(offset);
    sm_config_set_in_pins(&c, sampler->pin);
    
    const float cycles_per_sample = 2.03125f; // !
    float div = 1.0; // sampling as fast as we can 
    sm_config_set_clkdiv(&c, div);

    double achieved_sm_clock = (double)clock_get_hz(clk_sys) / (double)div;
    double achieved_sample_rate = achieved_sm_clock / (double)cycles_per_sample;
    // printf("PIO clkdiv=%.6f, SM clock=%.0f Hz, sample_rate=%.2f Hz\n", div, achieved_sm_clock, achieved_sample_rate);
    
    // Shift register
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    
    pio_sm_init(sampler->pio, sm, offset, &c);

    // DMA init
    dma_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    return achieved_sample_rate;
}

void start_capture(sampler_t *sampler) {
    capture_complete = false;
    memset((void*)sampler->sample_buffer, 0, sampler->buffer_size * sizeof(uint32_t));
    
    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, pio_get_dreq(sampler->pio, 0, false));
    
    dma_channel_configure(
        dma_channel,
        &config,
        sampler->sample_buffer,
        &sampler->pio->rxf[0],
        sampler->buffer_size,
        true
    );
    
    // run PIO state machine
    pio_sm_clear_fifos(sampler->pio, 0);
    pio_sm_restart(sampler->pio, 0);
    pio_sm_set_enabled(sampler->pio, 0, true);
}

void wait_capture_blocking(sampler_t *sampler) {
    dma_channel_wait_for_finish_blocking(dma_channel);
}

void stop_capture(sampler_t *sampler) {
    pio_sm_set_enabled(sampler->pio, 0, false);
    if (!capture_complete) {
        capture_complete = true;
        dma_channel_abort(dma_channel);
        dma_channel_acknowledge_irq0(dma_channel);
    }
}