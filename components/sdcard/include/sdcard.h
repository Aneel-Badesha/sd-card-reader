#ifndef SDCARD_H
#define SDCARD_H

#include "esp_err.h"
#include <stdint.h>

// Initialize the micro sd card
esp_err_t sdcard_init(void);

// Read from the sd card
esp_err_t sdcard_read_file(const char *path);

// Write to the sd card
esp_err_t sdcard_write_file(const char *path, char *data);

// Deinitialize the micro sd card
esp_err_t sdcard_deinit(void);

#endif // SDCARD_H
