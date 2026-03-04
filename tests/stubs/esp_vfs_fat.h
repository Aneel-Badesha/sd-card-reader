#pragma once

#include "driver/sdspi_host.h"
#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    bool format_if_mount_failed;
    int max_files;
    int allocation_unit_size;
} esp_vfs_fat_sdmmc_mount_config_t;

esp_err_t esp_vfs_fat_sdspi_mount(const char *base_path, sdmmc_host_t *host_config,
                                  const sdspi_device_config_t *slot_config,
                                  const esp_vfs_fat_sdmmc_mount_config_t *mount_config, sdmmc_card_t **out_card);

esp_err_t esp_vfs_fat_sdcard_unmount(const char *base_path, sdmmc_card_t *card);
