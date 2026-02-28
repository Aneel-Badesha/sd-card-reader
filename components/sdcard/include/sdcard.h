#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>
#include "esp_err.h"

// Initialize the micro sd card
esp_err_t sdcard_init(void);

// Read from the sd card
esp_err_t sdcard_read_file();

// Write to the sd card
esp_err_t sdcard_write_file();

// Deinitialize the micro sd card
esp_err_t sdcard_deinit(void);

#endif // SDCARD_H