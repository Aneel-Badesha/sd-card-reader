#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  esp_err                                                            */
/* ------------------------------------------------------------------ */

const char *esp_err_to_name(esp_err_t code)
{
    switch (code) {
    case 0:
        return "ESP_OK";
    case -1:
        return "ESP_FAIL";
    case 0x101:
        return "ESP_ERR_NO_MEM";
    case 0x102:
        return "ESP_ERR_INVALID_ARG";
    case 0x103:
        return "ESP_ERR_INVALID_STATE";
    case 0x107:
        return "ESP_ERR_TIMEOUT";
    default:
        return "UNKNOWN_ERROR";
    }
}

/* ------------------------------------------------------------------ */
/*  GPIO stubs                                                         */
/* ------------------------------------------------------------------ */

static int s_gpio_level = 1;
static esp_err_t s_gpio_config_rc = 0;

void stub_set_gpio_level(int level)
{
    s_gpio_level = level;
}

void stub_set_gpio_config_rc(esp_err_t rc)
{
    s_gpio_config_rc = rc;
}

esp_err_t gpio_config(const gpio_config_t *cfg)
{
    return s_gpio_config_rc;
}

int gpio_get_level(gpio_num_t gpio_num)
{
    return s_gpio_level;
}

/* ------------------------------------------------------------------ */
/*  FreeRTOS stubs                                                     */
/* ------------------------------------------------------------------ */

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return (SemaphoreHandle_t)1;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks)
{
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t sem)
{
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t sem)
{
}

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack_depth, void *params, UBaseType_t priority,
                       TaskHandle_t *handle)
{
    if (handle) {
        *handle = (TaskHandle_t)1;
    }
    return pdPASS;
}

void vTaskDelete(TaskHandle_t task)
{
}

void vTaskDelay(TickType_t ticks)
{
}

uint32_t ulTaskNotifyTake(BaseType_t clear_on_exit, TickType_t ticks_to_wait)
{
    return 0;
}

void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *higher_priority_task_woken)
{
    if (higher_priority_task_woken) {
        *higher_priority_task_woken = pdFALSE;
    }
}

/* ------------------------------------------------------------------ */
/*  ADC stubs                                                          */
/* ------------------------------------------------------------------ */

esp_err_t adc_continuous_new_handle(const adc_continuous_handle_cfg_t *hdl_config, adc_continuous_handle_t *ret_handle)
{
    if (ret_handle) {
        *ret_handle = (adc_continuous_handle_t)1;
    }
    return ESP_OK;
}

esp_err_t adc_continuous_config(adc_continuous_handle_t handle, const adc_continuous_config_t *config)
{
    return ESP_OK;
}

esp_err_t adc_continuous_register_event_callbacks(adc_continuous_handle_t handle, const adc_continuous_evt_cbs_t *cbs,
                                                  void *user_data)
{
    return ESP_OK;
}

esp_err_t adc_continuous_start(adc_continuous_handle_t handle)
{
    return ESP_OK;
}

esp_err_t adc_continuous_stop(adc_continuous_handle_t handle)
{
    return ESP_OK;
}

esp_err_t adc_continuous_deinit(adc_continuous_handle_t handle)
{
    return ESP_OK;
}

esp_err_t adc_continuous_read(adc_continuous_handle_t handle, uint8_t *buf, uint32_t length_max, uint32_t *out_length,
                              uint32_t timeout_ms)
{
    // Return timeout so the read loop in the task exits cleanly
    return ESP_ERR_TIMEOUT;
}

/* ------------------------------------------------------------------ */
/*  SPI / SD stubs                                                     */
/* ------------------------------------------------------------------ */

esp_err_t spi_bus_initialize(int host_id, const spi_bus_config_t *bus_config, int dma_chan)
{
    return ESP_OK;
}

esp_err_t spi_bus_free(int host_id)
{
    return ESP_OK;
}

esp_err_t esp_vfs_fat_sdspi_mount(const char *base_path, sdmmc_host_t *host_config,
                                  const sdspi_device_config_t *slot_config,
                                  const esp_vfs_fat_sdmmc_mount_config_t *mount_config, sdmmc_card_t **out_card)
{
    return ESP_OK;
}

esp_err_t esp_vfs_fat_sdcard_unmount(const char *base_path, sdmmc_card_t *card)
{
    return ESP_OK;
}

void sdmmc_card_print_info(FILE *stream, const sdmmc_card_t *card)
{
}
