#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "button.h"
#include "thumbstick.h"

static const char *TAG = "sd_card_reader";

#define GPIO_RED_BUTTON 17
#define GPIO_GREEN_BUTTON 16

static TaskHandle_t s_task_handle_button = NULL;
static SemaphoreHandle_t s_mutex_button = NULL;
static int s_button_values[2];

void s_button_task()
{
    s_mutex_button = xSemaphoreCreateMutex();
    if (s_mutex_button == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }

    esp_err_t rc = init_button(GPIO_GREEN_BUTTON);
    if (rc != ESP_OK) {
        vSemaphoreDelete(s_mutex_button);
        s_mutex_button = NULL;
        ESP_LOGE(TAG, "Failed to init green button");
        return;
    }

    rc = init_button(GPIO_RED_BUTTON);
    if(rc != ESP_OK)
    {
        vSemaphoreDelete(s_mutex_button);
        s_mutex_button = NULL;
        ESP_LOGE(TAG, "Failed to init green button");
        return;
    }

    while(1)    {
        xSemaphoreTake(s_mutex_button, pdMS_TO_TICKS(10));
        s_button_values[0] = button_is_pressed(GPIO_GREEN_BUTTON);
        s_button_values[1] = button_is_pressed(GPIO_RED_BUTTON);
        ESP_LOGD(TAG, "Green button value: %d, red button value: %d", s_button_values[0], s_button_values[1]);
        xSemaphoreGive(s_mutex_button);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

}

void app_main(void)
{
    ESP_LOGI(TAG, "SD card reader starting...");
    
    BaseType_t task_rc = xTaskCreate(s_button_task, "button", 4096, NULL, 5, &s_task_handle_button);
    if (task_rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        vSemaphoreDelete(s_mutex_button);
        s_mutex_button = NULL;
        return;
    }

    esp_err_t rc = thumbstick_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init thumbstick");
        vSemaphoreDelete(s_mutex_button);
        s_mutex_button = NULL;
        return;
    }

    while(1)    {
        uint32_t x_out = 0;
        uint32_t y_out = 0;

        rc = thumbstick_get_values(&x_out, &y_out);
        if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init thumbstick");
            vSemaphoreDelete(s_mutex_button);
            s_mutex_button = NULL;
            return;
        }

        ESP_LOGD(TAG, "X value: %"PRIu32", Y value: %"PRIu32, x_out, y_out);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
        
}
