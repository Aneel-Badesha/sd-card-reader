#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

// Initialize button
esp_err_t init_button(gpio_num_t gpio_num);

// Return true if the button is currently pressed
bool button_is_pressed(gpio_num_t gpio_num);

#endif // BUTTON_H
