#include "thumbstick.h"

#include <string.h>
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"

static const char *TAG = "thumbstick";

/* ------------------------------------------------------------------ */
/*  Configuration                                                       */
/* ------------------------------------------------------------------ */

#define THUMBSTICK_ADC_UNIT         ADC_UNIT_1
#define THUMBSTICK_ADC_CONV_MODE    ADC_CONV_SINGLE_UNIT_1
#define THUMBSTICK_ADC_ATTEN        ADC_ATTEN_DB_12
#define THUMBSTICK_ADC_BIT_WIDTH    SOC_ADC_DIGI_MAX_BITWIDTH
#define THUMBSTICK_READ_LEN         256
#define THUMBSTICK_CHANNEL_SIZE     2

/* X axis = CH0, Y axis = CH2 */
static const adc_channel_t s_channels[THUMBSTICK_CHANNEL_SIZE] = {
    ADC_CHANNEL_0,
    ADC_CHANNEL_2
};

/* ------------------------------------------------------------------ */
/*  Module-level state                                                  */
/* ------------------------------------------------------------------ */

static adc_continuous_handle_t s_adc_handle  = NULL;
static TaskHandle_t             s_task_handle = NULL;
static SemaphoreHandle_t        s_mutex       = NULL;
static uint32_t                 s_x_value     = 0;
static uint32_t                 s_y_value     = 0;
static bool                     s_initialized = false;

/* ------------------------------------------------------------------ */
/*  ISR callback – notifies the reader task that a frame is ready      */
/* ------------------------------------------------------------------ */

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle,
                                     const adc_continuous_evt_data_t *edata,
                                     void *user_data)
{
    BaseType_t must_yield = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &must_yield);
    return (must_yield == pdTRUE);
}

/* ------------------------------------------------------------------ */
/*  Internal ADC init helper                                            */
/* ------------------------------------------------------------------ */

static esp_err_t s_adc_init(void)
{
    esp_err_t rc;

    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = 1024,
        .conv_frame_size    = THUMBSTICK_READ_LEN,
    };
    rc = adc_continuous_new_handle(&handle_cfg, &s_adc_handle);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "adc_continuous_new_handle failed: %s", esp_err_to_name(rc));
        return rc;
    }

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    for (int i = 0; i < THUMBSTICK_CHANNEL_SIZE; i++) {
        adc_pattern[i].atten     = THUMBSTICK_ADC_ATTEN;
        adc_pattern[i].channel   = s_channels[i] & 0x7;
        adc_pattern[i].unit      = THUMBSTICK_ADC_UNIT;
        adc_pattern[i].bit_width = THUMBSTICK_ADC_BIT_WIDTH;
        ESP_LOGD(TAG, "adc_pattern[%d]: atten=0x%"PRIx8" ch=0x%"PRIx8" unit=0x%"PRIx8,
                 i, adc_pattern[i].atten, adc_pattern[i].channel, adc_pattern[i].unit);
    }

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,
        .conv_mode      = THUMBSTICK_ADC_CONV_MODE,
        .pattern_num    = THUMBSTICK_CHANNEL_SIZE,
        .adc_pattern    = adc_pattern,
    };
    rc = adc_continuous_config(s_adc_handle, &dig_cfg);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "adc_continuous_config failed: %s", esp_err_to_name(rc));
        adc_continuous_deinit(s_adc_handle);
        s_adc_handle = NULL;
        return rc;
    }

    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  Background reader task                                              */
/* ------------------------------------------------------------------ */

static void s_thumbstick_task(void *arg)
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[THUMBSTICK_READ_LEN];

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (1) {
            memset(result, 0, sizeof(result));
            ret = adc_continuous_read(s_adc_handle, result, THUMBSTICK_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK) {
                adc_digi_output_data_t *raw = (adc_digi_output_data_t *)result;
                uint32_t num_samples = ret_num / SOC_ADC_DIGI_RESULT_BYTES;

                uint32_t new_x = s_x_value;
                uint32_t new_y = s_y_value;

                for (uint32_t i = 0; i < num_samples; i++) {
                    uint8_t  ch  = raw[i].type1.channel;
                    uint32_t val = raw[i].type1.data;

                    if (ch == s_channels[0]) {
                        new_x = val;
                    } else if (ch == s_channels[1]) {
                        new_y = val;
                    } else {
                        ESP_LOGD(TAG, "Unexpected channel %d val=%"PRIu32, ch, val);
                    }
                }

                if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    s_x_value = new_x;
                    s_y_value = new_y;
                    xSemaphoreGive(s_mutex);
                }

                vTaskDelay(1);
            } else if (ret == ESP_ERR_TIMEOUT) {
                break;
            } else {
                ESP_LOGE(TAG, "adc_continuous_read error: %s", esp_err_to_name(ret));
                break;
            }
        }
    }

    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t thumbstick_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "thumbstick_init called more than once");
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t rc = s_adc_init();
    if (rc != ESP_OK) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return rc;
    }

    /* Create the task before registering the callback so s_task_handle
       is valid when the first ISR fires after adc_continuous_start. */
    BaseType_t task_rc = xTaskCreate(s_thumbstick_task, "thumbstick", 4096, NULL, 5, &s_task_handle);
    if (task_rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create thumbstick task");
        adc_continuous_deinit(s_adc_handle);
        s_adc_handle = NULL;
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    rc = adc_continuous_register_event_callbacks(s_adc_handle, &cbs, NULL);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "register_event_callbacks failed: %s", esp_err_to_name(rc));
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
        adc_continuous_deinit(s_adc_handle);
        s_adc_handle = NULL;
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return rc;
    }

    rc = adc_continuous_start(s_adc_handle);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "adc_continuous_start failed: %s", esp_err_to_name(rc));
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
        adc_continuous_deinit(s_adc_handle);
        s_adc_handle = NULL;
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return rc;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Thumbstick initialized");
    return ESP_OK;
}

esp_err_t thumbstick_get_values(uint32_t *x_out, uint32_t *y_out)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "thumbstick_get_values called before thumbstick_init");
        return ESP_ERR_INVALID_STATE;
    }
    if (x_out == NULL || y_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }
    *x_out = s_x_value;
    *y_out = s_y_value;
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t thumbstick_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    s_initialized = false;

    if (s_adc_handle != NULL) {
        adc_continuous_stop(s_adc_handle);
        adc_continuous_deinit(s_adc_handle);
        s_adc_handle = NULL;
    }

    if (s_task_handle != NULL) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    ESP_LOGI(TAG, "Thumbstick deinitialized");
    return ESP_OK;
}
