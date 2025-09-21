#pragma once

#include "pico/stdlib.h"
#include "hardware/i2c.h"

typedef struct {
    i2c_inst_t *i2c_port;
    uint8_t address;
    uint8_t width;
    uint8_t height;
    bool external_vcc;
    uint8_t buffer[1024]; // 128x64/8
} ssd1306_t;

void ssd1306_init(ssd1306_t *disp, uint8_t width, uint8_t height, uint8_t address, i2c_inst_t *i2c_port);
void ssd1306_clear(ssd1306_t *disp);
void ssd1306_show(ssd1306_t *disp);
void ssd1306_draw_pixel(ssd1306_t *disp, uint8_t x, uint8_t y, bool on);
void ssd1306_draw_string(ssd1306_t *disp, uint8_t x, uint8_t y, uint8_t scale, const char *str);
void ssd1306_draw_line(ssd1306_t *disp, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool on);
void ssd1306_draw_rect(ssd1306_t *disp, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);
