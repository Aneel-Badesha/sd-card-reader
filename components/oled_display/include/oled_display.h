#pragma once

#include "esp_err.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Color constants (RGB565)                                           */
/* ------------------------------------------------------------------ */

#define OLED_BLACK 0x0000
#define OLED_WHITE 0xFFFF
#define OLED_RED 0xF800
#define OLED_GREEN 0x07E0
#define OLED_BLUE 0x001F
#define OLED_YELLOW 0xFFE0
#define OLED_CYAN 0x07FF
#define OLED_MAGENTA 0xF81F

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

esp_err_t oled_init(void);
void oled_clear(uint16_t color);
void oled_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void oled_print_string(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg);
esp_err_t oled_deinit(void);
