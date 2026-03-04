#pragma once

#include "esp_err.h"

typedef struct {
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int max_transfer_sz;
} spi_bus_config_t;

esp_err_t spi_bus_initialize(int host_id, const spi_bus_config_t *bus_config, int dma_chan);
esp_err_t spi_bus_free(int host_id);
