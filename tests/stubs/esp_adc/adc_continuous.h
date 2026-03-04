#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdint.h>

#define SOC_ADC_PATT_LEN_MAX 10
#define SOC_ADC_DIGI_RESULT_BYTES 4
#define SOC_ADC_DIGI_MAX_BITWIDTH 12

typedef enum {
    ADC_UNIT_1 = 0,
} adc_unit_t;

typedef enum {
    ADC_CHANNEL_0 = 0,
    ADC_CHANNEL_2 = 2,
} adc_channel_t;

typedef enum {
    ADC_ATTEN_DB_12 = 3,
} adc_atten_t;

typedef enum {
    ADC_CONV_SINGLE_UNIT_1 = 1,
} adc_conv_unit_t;

typedef void *adc_continuous_handle_t;

typedef struct {
    uint32_t max_store_buf_size;
    uint32_t conv_frame_size;
} adc_continuous_handle_cfg_t;

typedef struct {
    adc_atten_t atten;
    adc_channel_t channel;
    adc_unit_t unit;
    uint32_t bit_width;
} adc_digi_pattern_config_t;

typedef struct {
    uint32_t sample_freq_hz;
    adc_conv_unit_t conv_mode;
    uint32_t pattern_num;
    adc_digi_pattern_config_t *adc_pattern;
} adc_continuous_config_t;

typedef struct {
    uint32_t size;
} adc_continuous_evt_data_t;

typedef bool (*adc_continuous_callback_t)(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata,
                                          void *user_data);

typedef struct {
    adc_continuous_callback_t on_conv_done;
} adc_continuous_evt_cbs_t;

// Output data: union lets us access .type1.channel and .type1.data
typedef union {
    struct {
        uint32_t data : 12;
        uint32_t channel : 4;
        uint32_t unit : 2;
        uint32_t : 14;
    } type1;
    uint32_t val;
} adc_digi_output_data_t;

esp_err_t adc_continuous_new_handle(const adc_continuous_handle_cfg_t *hdl_config, adc_continuous_handle_t *ret_handle);
esp_err_t adc_continuous_config(adc_continuous_handle_t handle, const adc_continuous_config_t *config);
esp_err_t adc_continuous_register_event_callbacks(adc_continuous_handle_t handle, const adc_continuous_evt_cbs_t *cbs,
                                                  void *user_data);
esp_err_t adc_continuous_start(adc_continuous_handle_t handle);
esp_err_t adc_continuous_stop(adc_continuous_handle_t handle);
esp_err_t adc_continuous_deinit(adc_continuous_handle_t handle);
esp_err_t adc_continuous_read(adc_continuous_handle_t handle, uint8_t *buf, uint32_t length_max, uint32_t *out_length,
                              uint32_t timeout_ms);
