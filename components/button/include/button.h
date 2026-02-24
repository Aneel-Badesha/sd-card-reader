#ifndef BUTTON_H
#define BUTTON_H

#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "driver/gpio.h"

// Initialize button
esp_err_t init_button(gpio_num_t gpio_num);

// Return button value
int get_button_value(gpio_num_t gpio_num);

#endif // BUTTON_H