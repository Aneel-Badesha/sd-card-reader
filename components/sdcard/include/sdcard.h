#ifndef SDCARD_H
#define SDCARD_H

#include "esp_err.h"
#include <stdint.h>

// Initialize SPI3 bus (call once from app_main before sdcard_init or ethernet_init)
esp_err_t sdcard_bus_init(void);

// Initialize the micro sd card (requires sdcard_bus_init to have been called)
esp_err_t sdcard_init(void);

// Read from the sd card
esp_err_t sdcard_read_file(const char *path);

// Write to the sd card
esp_err_t sdcard_write_file(const char *path, const char *data);

// Deinitialize the micro sd card
esp_err_t sdcard_deinit(void);

#endif // SDCARD_H
