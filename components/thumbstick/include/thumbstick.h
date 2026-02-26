#ifndef THUMBSTICK_H
#define THUMBSTICK_H

#include <stdint.h>
#include "esp_err.h"

// Initialize thumbstick ADC and start internal background read task
esp_err_t thumbstick_init(void);

// Copy the latest X and Y axis raw ADC values into the provided pointers
esp_err_t thumbstick_get_values(uint32_t *x_out, uint32_t *y_out);

// Stop the background task and release ADC resources
esp_err_t thumbstick_deinit(void);

#endif // THUMBSTICK_H
