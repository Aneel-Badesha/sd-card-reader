#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "button.h"

static const char *TAG = "sd_card_reader";

#define GPIO_RED_BUTTON 2
#define GPIO_GREEN_BUTTON 4

void app_main(void)
{
    ESP_LOGI(TAG, "SD card reader starting...");

    esp_err_t rc = ESP_OK;
    rc = init_button(GPIO_GREEN_BUTTON);
    if(rc != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init green button");
    }

    rc = init_button(GPIO_RED_BUTTON);
    if(rc != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init red button");
    }
}
