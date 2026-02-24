#include "button.h"

static const char *TAG = "button";

// Initialize button
esp_err_t init_button(gpio_num_t gpio_num)
{
    esp_err_t rc = ESP_OK;

    gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_DEF_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    rc = gpio_config(&gpio_cfg);
    if(rc != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize button");
        return rc;
    }

    return rc;
}

// Read button value
int get_button_value(gpio_num_t gpio_num)
{
    return gpio_get_level(gpio_num);
}