#include "sdcard.h"

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "sdcard";

#define SD_CARD_CS 5
#define SD_CARD_SCLK 18
#define SD_CARD_MISO 19
#define SD_CARD_MOSI 23
#define MOUNT_POINT "/sdcard"

// Module-level state so deinit can access them
static sdmmc_card_t *s_card = NULL;
static sdmmc_host_t s_host;

esp_err_t sdcard_bus_init(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_CARD_MOSI,
        .miso_io_num = SD_CARD_MISO,
        .sclk_io_num = SD_CARD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t rc = spi_bus_initialize(SPI3_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI3 bus: %s", esp_err_to_name(rc));
        return rc;
    }

    ESP_LOGI(TAG, "SPI3 bus initialized");
    return ESP_OK;
}

esp_err_t sdcard_init(void)
{
    esp_err_t rc;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    ESP_LOGI(TAG, "Initializing SD card");
    s_host = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
    s_host.slot = SPI3_HOST;    // GPIO 18/19/23/5 are native VSPI pins
    s_host.max_freq_khz = 4000; // 4 MHz — reduces CRC errors on breadboard wiring

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CARD_CS;
    slot_config.host_id = s_host.slot;

    ESP_LOGI(TAG, "Mounting filesystem at %s", MOUNT_POINT);
    rc = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &s_host, &slot_config, &mount_config, &s_card);
    if (rc != ESP_OK) {
        if (rc == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialize card: %s", esp_err_to_name(rc));
        }
        return rc;
    }

    ESP_LOGI(TAG, "Filesystem mounted");
    sdmmc_card_print_info(stdout, s_card);

    return ESP_OK;
}

esp_err_t sdcard_read_file(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    char line[256];
    fgets(line, sizeof(line), f);
    fclose(f);

    // Strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

esp_err_t sdcard_write_file(const char *path, const char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing (errno %d)", errno);
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

esp_err_t sdcard_deinit(void)
{
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
    s_card = NULL;
    ESP_LOGI(TAG, "Card unmounted");
    // SPI3 bus is shared with W5500 — do not free it here
    return ESP_OK;
}
