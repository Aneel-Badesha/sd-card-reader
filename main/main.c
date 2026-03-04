#include "button.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdcard.h"
#include "thumbstick.h"
#include <inttypes.h>
#include <stdio.h>

static const char *TAG = "sd_card_reader";

#define GPIO_RED_BUTTON 17
#define GPIO_GREEN_BUTTON 16

static TaskHandle_t s_task_handle_button = NULL;
static TaskHandle_t s_task_handle_thumbstick = NULL;
static TaskHandle_t s_task_handle_sd_card = NULL;
static SemaphoreHandle_t s_mutex_button = NULL;
static int s_button_values[2];

void s_button_task(void *pvParameters)
{
    s_mutex_button = xSemaphoreCreateMutex();
    if (s_mutex_button == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        vTaskDelete(NULL);
        return;
    }

    esp_err_t rc = init_button(GPIO_GREEN_BUTTON);
    if (rc != ESP_OK) {
        vSemaphoreDelete(s_mutex_button);
        s_mutex_button = NULL;
        ESP_LOGE(TAG, "Failed to init green button");
        vTaskDelete(NULL);
        return;
    }

    rc = init_button(GPIO_RED_BUTTON);
    if (rc != ESP_OK) {
        vSemaphoreDelete(s_mutex_button);
        s_mutex_button = NULL;
        ESP_LOGE(TAG, "Failed to init red button");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        xSemaphoreTake(s_mutex_button, pdMS_TO_TICKS(10));
        s_button_values[0] = button_is_pressed(GPIO_GREEN_BUTTON);
        s_button_values[1] = button_is_pressed(GPIO_RED_BUTTON);
        ESP_LOGD(TAG, "Green button value: %d, red button value: %d", s_button_values[0], s_button_values[1]);
        xSemaphoreGive(s_mutex_button);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void s_thumbstick_task(void *pvParameters)
{
    esp_err_t rc = thumbstick_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init thumbstick");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        uint32_t x_out = 0;
        uint32_t y_out = 0;

        rc = thumbstick_get_values(&x_out, &y_out);
        if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get thumbstick values");
            vTaskDelete(NULL);
            return;
        }

        ESP_LOGD(TAG, "X value: %" PRIu32 ", Y value: %" PRIu32, x_out, y_out);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void s_sd_card_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(500)); // wait for SD card VCC to stabilise
    esp_err_t rc = sdcard_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init sd card");
        vTaskDelete(NULL);
        return;
    }

    int64_t us = esp_timer_get_time();
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "uptime: %lld.%06lld s\n", us / 1000000, us % 1000000);

    rc = sdcard_write_file("/sdcard/timestamp.txt", timestamp);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write timestamp file");
    } else {
        ESP_LOGI(TAG, "Wrote timestamp: %s", timestamp);
    }

    rc = sdcard_deinit();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinit sd card");
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "SD card reader starting...");

    BaseType_t task_rc_button = xTaskCreate(s_button_task, "button", 4096, NULL, 5, &s_task_handle_button);
    if (task_rc_button != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        vSemaphoreDelete(s_mutex_button);
        s_mutex_button = NULL;
        return;
    }

    BaseType_t task_rc_thumbstick =
        xTaskCreate(s_thumbstick_task, "thumbstick", 4096, NULL, 5, &s_task_handle_thumbstick);
    if (task_rc_thumbstick != pdPASS) {
        ESP_LOGE(TAG, "Failed to create thumbstick task");
        return;
    }

    BaseType_t task_rc_sd_card = xTaskCreate(s_sd_card_task, "sdcard", 4096, NULL, 5, &s_task_handle_sd_card);
    if (task_rc_sd_card != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sdcard task");
        return;
    }
}
