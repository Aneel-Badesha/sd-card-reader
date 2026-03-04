#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdio.h>

// Minimal sdmmc types needed by sdcard component
typedef struct {
    struct {
        char name[8];
    } cid;
} sdmmc_card_t;

typedef struct {
    int slot;
    int max_freq_khz;
} sdmmc_host_t;

typedef struct {
    int gpio_cs;
    int host_id;
} sdspi_device_config_t;

#define SDSPI_HOST_DEFAULT() {.slot = 1, .max_freq_khz = 20000}

#define SDSPI_DEVICE_CONFIG_DEFAULT() {.gpio_cs = 0, .host_id = 1}

#define SDSPI_DEFAULT_DMA 1
