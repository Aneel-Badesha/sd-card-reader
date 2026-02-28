#include "sdcard.h"

#include <string.h>
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/sdspi_host.h"

static const char *TAG = "sdcard";

#define SD_CARD_CS    5
#define SD_CARD_SCLK 18
#define SD_CARD_MISO 19
#define SD_CARD_MOSI 23

esp_err_t sdcard_init(void);
{
    esp_err_t rc = sdspi_host();
    if(rc != ESP_OK)    {
        ESP_LOGE(TAG,"Failed to init SD SPI Host");
        return rc;
    }


}

esp_err_t sdcard_read_file();
{
    
}

esp_err_t sdcard_write_file();
{
    
}

esp_err_t sdcard_deinit(void)
{

}