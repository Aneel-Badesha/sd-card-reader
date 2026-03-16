#include "oled_display.h"
#include "ssd1351_font.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "oled_display";

/* ------------------------------------------------------------------ */
/*  Pin definitions                                                    */
/* ------------------------------------------------------------------ */

#define OLED_MOSI 13
#define OLED_SCLK 14
#define OLED_CS 15
#define OLED_DC 2
#define OLED_RST 4

#define OLED_WIDTH 128
#define OLED_HEIGHT 128

/* ------------------------------------------------------------------ */
/*  SSD1351 command codes                                              */
/* ------------------------------------------------------------------ */

#define SSD1351_CMD_SETCOLUMN 0x15
#define SSD1351_CMD_SETROW 0x75
#define SSD1351_CMD_WRITERAM 0x5C
#define SSD1351_CMD_SETREMAP 0xA0
#define SSD1351_CMD_STARTLINE 0xA1
#define SSD1351_CMD_DISPLAYOFFSET 0xA2
#define SSD1351_CMD_DISPLAYALLOFF 0xA4
#define SSD1351_CMD_NORMALDISPLAY 0xA6
#define SSD1351_CMD_FUNCTIONSELECT 0xAB
#define SSD1351_CMD_DISPLAYOFF 0xAE
#define SSD1351_CMD_DISPLAYON 0xAF
#define SSD1351_CMD_PRECHARGE 0xB1
#define SSD1351_CMD_DISPLAYENHANCE 0xB2
#define SSD1351_CMD_CLOCKDIV 0xB3
#define SSD1351_CMD_SETVSL 0xB4
#define SSD1351_CMD_SETGPIO 0xB5
#define SSD1351_CMD_PRECHARGE2 0xB6
#define SSD1351_CMD_PRECHARGELEVEL 0xBB
#define SSD1351_CMD_VCOMH 0xBE
#define SSD1351_CMD_CONTRASTABC 0xC1
#define SSD1351_CMD_CONTRASTMASTER 0xC7
#define SSD1351_CMD_MUXRATIO 0xCA
#define SSD1351_CMD_COMMANDLOCK 0xFD

/* ------------------------------------------------------------------ */
/*  Module-level state                                                 */
/* ------------------------------------------------------------------ */

static spi_device_handle_t s_spi = NULL;

/* ------------------------------------------------------------------ */
/*  Low-level SPI helpers                                              */
/* ------------------------------------------------------------------ */

static void s_write_cmd(uint8_t cmd)
{
    gpio_set_level(OLED_DC, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(s_spi, &t);
}

static void s_write_data(const uint8_t *buf, size_t len)
{
    if (len == 0) {
        return;
    }
    gpio_set_level(OLED_DC, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = buf,
    };
    spi_device_polling_transmit(s_spi, &t);
}

static void s_write_data_byte(uint8_t byte)
{
    s_write_data(&byte, 1);
}

/* ------------------------------------------------------------------ */
/*  Window address helper                                              */
/* ------------------------------------------------------------------ */

static void s_set_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    s_write_cmd(SSD1351_CMD_SETCOLUMN);
    s_write_data_byte(x);
    s_write_data_byte(x + w - 1);

    s_write_cmd(SSD1351_CMD_SETROW);
    s_write_data_byte(y);
    s_write_data_byte(y + h - 1);

    s_write_cmd(SSD1351_CMD_WRITERAM);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

esp_err_t oled_init(void)
{
    esp_err_t rc;

    // Configure DC and RST as outputs
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << OLED_DC) | (1ULL << OLED_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    rc = gpio_config(&io_cfg);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(rc));
        return rc;
    }

    // Hardware reset: RST low 500ms, then high 500ms
    gpio_set_level(OLED_RST, 1);
    gpio_set_level(OLED_DC, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(OLED_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(OLED_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Initialise SPI bus (SPI2_HOST / HSPI)
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = OLED_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = OLED_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = OLED_WIDTH * OLED_HEIGHT * 2,
    };
    rc = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(rc));
        return rc;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
        .mode = 0,                          // SPI mode 0 (CPOL=0, CPHA=0)
        .spics_io_num = OLED_CS,
        .queue_size = 1,
    };
    rc = spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(rc));
        spi_bus_free(SPI2_HOST);
        return rc;
    }

    // SSD1351 initialisation sequence
    s_write_cmd(SSD1351_CMD_COMMANDLOCK);
    s_write_data_byte(0x12); // unlock

    s_write_cmd(SSD1351_CMD_COMMANDLOCK);
    s_write_data_byte(0xB1); // unlock all accessible commands

    s_write_cmd(SSD1351_CMD_DISPLAYOFF);

    s_write_cmd(SSD1351_CMD_DISPLAYALLOFF);

    s_write_cmd(SSD1351_CMD_SETCOLUMN);
    s_write_data_byte(0x00);
    s_write_data_byte(0x7F);

    s_write_cmd(SSD1351_CMD_SETROW);
    s_write_data_byte(0x00);
    s_write_data_byte(0x7F);

    s_write_cmd(SSD1351_CMD_CLOCKDIV);
    s_write_data_byte(0xF1);

    s_write_cmd(SSD1351_CMD_MUXRATIO);
    s_write_data_byte(0x7F);

    s_write_cmd(SSD1351_CMD_SETREMAP);
    s_write_data_byte(0x74); // horizontal increment, RGB565, COM split, color swap

    s_write_cmd(SSD1351_CMD_STARTLINE);
    s_write_data_byte(0x00);

    s_write_cmd(SSD1351_CMD_DISPLAYOFFSET);
    s_write_data_byte(0x00);

    s_write_cmd(SSD1351_CMD_FUNCTIONSELECT);
    s_write_data_byte(0x01); // internal VDD regulator

    s_write_cmd(SSD1351_CMD_SETVSL);
    s_write_data_byte(0xA0);
    s_write_data_byte(0xB5);
    s_write_data_byte(0x55);

    s_write_cmd(SSD1351_CMD_CONTRASTABC);
    s_write_data_byte(0xC8);
    s_write_data_byte(0x80);
    s_write_data_byte(0xC0);

    s_write_cmd(SSD1351_CMD_CONTRASTMASTER);
    s_write_data_byte(0x0F);

    s_write_cmd(SSD1351_CMD_PRECHARGE);
    s_write_data_byte(0x32);

    s_write_cmd(SSD1351_CMD_DISPLAYENHANCE);
    s_write_data_byte(0xA4);
    s_write_data_byte(0x00);
    s_write_data_byte(0x00);

    s_write_cmd(SSD1351_CMD_PRECHARGELEVEL);
    s_write_data_byte(0x17);

    s_write_cmd(SSD1351_CMD_PRECHARGE2);
    s_write_data_byte(0x01);

    s_write_cmd(SSD1351_CMD_VCOMH);
    s_write_data_byte(0x05);

    s_write_cmd(SSD1351_CMD_NORMALDISPLAY);

    oled_clear(OLED_BLACK);

    s_write_cmd(SSD1351_CMD_DISPLAYON);

    ESP_LOGI(TAG, "SSD1351 initialized");
    return ESP_OK;
}

void oled_clear(uint16_t color)
{
    oled_fill_rect(0, 0, OLED_WIDTH, OLED_HEIGHT, color);
}

void oled_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (x < 0 || y < 0 || w <= 0 || h <= 0) {
        return;
    }
    if (x + w > OLED_WIDTH) {
        w = OLED_WIDTH - x;
    }
    if (y + h > OLED_HEIGHT) {
        h = OLED_HEIGHT - y;
    }

    s_set_window((uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)h);

    // Build a row buffer and stream it h times
    uint8_t row[OLED_WIDTH * 2];
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    for (int i = 0; i < w; i++) {
        row[i * 2] = hi;
        row[i * 2 + 1] = lo;
    }

    gpio_set_level(OLED_DC, 1);
    for (int row_idx = 0; row_idx < h; row_idx++) {
        spi_transaction_t t = {
            .length = w * 16,
            .tx_buffer = row,
        };
        spi_device_polling_transmit(s_spi, &t);
    }
}

void oled_print_string(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg)
{
    if (!str) {
        return;
    }

    int16_t cursor_x = x;

    while (*str) {
        uint8_t c = (uint8_t)*str++;

        // Replace out-of-range characters with space
        if (c < FONT_5X8_FIRST || c > 0x7E) {
            c = ' ';
        }

        const uint8_t *glyph = FONT_5X8[c - FONT_5X8_FIRST];

        for (int col = 0; col < FONT_5X8_WIDTH; col++) {
            uint8_t col_data = glyph[col];
            for (int row = 0; row < FONT_5X8_HEIGHT; row++) {
                uint16_t pixel_color = (col_data & (1 << row)) ? fg : bg;
                oled_fill_rect(cursor_x + col, y + row, 1, 1, pixel_color);
            }
        }

        // 1-pixel gap between characters
        oled_fill_rect(cursor_x + FONT_5X8_WIDTH, y, 1, FONT_5X8_HEIGHT, bg);
        cursor_x += FONT_5X8_WIDTH + 1;

        // Stop if we've gone off-screen
        if (cursor_x >= OLED_WIDTH) {
            break;
        }
    }
}

esp_err_t oled_deinit(void)
{
    if (s_spi) {
        spi_bus_remove_device(s_spi);
        s_spi = NULL;
    }
    spi_bus_free(SPI2_HOST);
    ESP_LOGI(TAG, "OLED deinitialized");
    return ESP_OK;
}
