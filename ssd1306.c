#include "ssd1306.h"
#include "font.h"
#include "string.h"

void ssd1306_write_command(ssd1306_t *disp, uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_blocking(disp->i2c_port, disp->address, buf, 2, false);
}

void ssd1306_init(ssd1306_t *disp) {

    gpio_init(disp->SDA);
    gpio_init(disp->SCL);
    gpio_set_dir(disp->SDA, GPIO_IN);
    gpio_set_dir(disp->SCL, GPIO_IN);
    
    i2c_init(disp->i2c_port, 400000);
    
    gpio_set_function(disp->SDA, GPIO_FUNC_I2C);
    gpio_set_function(disp->SCL, GPIO_FUNC_I2C);
    
    gpio_pull_up(disp->SDA);
    gpio_pull_up(disp->SCL);
    
    sleep_ms(100);
    
    // disp->i2c_port = i2c_port;
    // disp->address = address;
    // disp->width = width;
    // disp->height = height;
//    disp->external_vcc = false;
    
    // Initialize sequence
    ssd1306_write_command(disp, 0xAE); // Display off
    ssd1306_write_command(disp, 0x20); // Memory mode
    ssd1306_write_command(disp, 0x00); // Horizontal
    ssd1306_write_command(disp, 0xB0); // Page start
    ssd1306_write_command(disp, 0xC8); // Scan direction
    ssd1306_write_command(disp, 0x00); // Low column
    ssd1306_write_command(disp, 0x10); // High column
    ssd1306_write_command(disp, 0x40); // Start line
    ssd1306_write_command(disp, 0x81); // Contrast
    ssd1306_write_command(disp, 0xFF);
    ssd1306_write_command(disp, 0xA1); // Segment remap
    ssd1306_write_command(disp, 0xA6); // Normal display
    ssd1306_write_command(disp, 0xA8); // Multiplex ratio
    ssd1306_write_command(disp, disp->height - 1);
    ssd1306_write_command(disp, 0xD3); // Display offset
    ssd1306_write_command(disp, 0x00);
    ssd1306_write_command(disp, 0xD5); // Clock divide
    ssd1306_write_command(disp, 0xF0);
    ssd1306_write_command(disp, 0xD9); // Precharge
    ssd1306_write_command(disp, 0x22);
    ssd1306_write_command(disp, 0xDA); // COM pins
    ssd1306_write_command(disp, 0x12);
    ssd1306_write_command(disp, 0xDB); // VCOM detect
    ssd1306_write_command(disp, 0x20);
    ssd1306_write_command(disp, 0x8D); // Charge pump
    ssd1306_write_command(disp, 0x14);
    ssd1306_write_command(disp, 0xAF); // Display on
    
    // ssd1306_clear(disp);
    //  ssd1306_write_command(disp, 0xAE); // Display off
    // ssd1306_write_command(disp, 0x20); // Memory mode
    // ssd1306_write_command(disp, 0x00); // Horizontal
    // ssd1306_write_command(disp, 0xB0); // Page start
    // // Add more initialization commands as needed
    // ssd1306_write_command(disp, 0xAF); // Display on
    
    // Clear display
    for (int i = 0; i < sizeof(disp->buffer); i++) {
        disp->buffer[i] = 0;
    }
}

void ssd1306_clear(ssd1306_t *disp) {
    for (int i = 0; i < sizeof(disp->buffer); i++) {
        disp->buffer[i] = 0;
    }
}

void ssd1306_fill(ssd1306_t *disp, uint8_t data) {
    for (int i = 0; i < sizeof(disp->buffer); i++) {
        disp->buffer[i] = data;
    }
}

void ssd1306_show(ssd1306_t *disp) {
    for (uint8_t page = 0; page < 8; page++) {
        ssd1306_write_command(disp, 0xB0 + page);
        ssd1306_write_command(disp, 0x00);
        ssd1306_write_command(disp, 0x10);
        
        uint8_t buf[129] = {0x40};
        memcpy(buf + 1, &disp->buffer[page * 128], 128);
        i2c_write_blocking(disp->i2c_port, disp->address, buf, 129, false);
    }
}

void ssd1306_draw_pixel(ssd1306_t *disp, uint8_t x, uint8_t y, bool on) {
    if (x >= disp->width || y >= disp->height) return;
    
    uint16_t index = x + (y / 8) * disp->width;
    if (on) {
        disp->buffer[index] |= (1 << (y % 8));
    } else {
        disp->buffer[index] &= ~(1 << (y % 8));
    }
}

void ssd1306_draw_char(ssd1306_t *disp, uint8_t x, uint8_t y, uint8_t scale, char c);

void ssd1306_draw_string(ssd1306_t *disp, uint8_t x, uint8_t y, uint8_t scale, const char *str) {
    while (*str) {
        ssd1306_draw_char(disp, x, y, scale, *str);
        x += 6 * scale;
        str++;
    }
}

void ssd1306_draw_char(ssd1306_t *disp, uint8_t x, uint8_t y, uint8_t scale, char c) {
    if (c < 32 || c > 127) return;
    
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t line = font[(c - 32) * 5 + i];
        for (uint8_t j = 0; j < 8; j++) {
            if (line & 0x1) {
                for (uint8_t s = 0; s < scale; s++) {
                    for (uint8_t t = 0; t < scale; t++) {
                        ssd1306_draw_pixel(disp, x + i * scale + s, y + j * scale + t, true);
                    }
                }
            }
            line >>= 1;
        }
    }
}
