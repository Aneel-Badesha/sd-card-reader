#include "button.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "oled_display.h"
#include "sdcard.h"
#include "thumbstick.h"
#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "sd_card_reader";

#define GPIO_RED_BUTTON 17
#define GPIO_GREEN_BUTTON 16

#define SD_MOUNT_POINT "/sdcard"

/* ------------------------------------------------------------------ */
/*  Task handles                                                       */
/* ------------------------------------------------------------------ */

static TaskHandle_t s_task_handle_button = NULL;
static TaskHandle_t s_task_handle_thumbstick = NULL;
static TaskHandle_t s_task_handle_sd_card = NULL;
static TaskHandle_t s_task_handle_oled = NULL;

/* ------------------------------------------------------------------ */
/*  Shared input state (written by button/thumbstick tasks)            */
/* ------------------------------------------------------------------ */

static SemaphoreHandle_t s_mutex_input = NULL;
static int s_button_green = 0;
static int s_button_red = 0;
static uint32_t s_thumbstick_x = 2048;
static uint32_t s_thumbstick_y = 2048;

/* Signal from SD task to OLED task that the card is mounted */
static volatile bool s_sd_ready = false;

/* ------------------------------------------------------------------ */
/*  File browser state                                                 */
/* ------------------------------------------------------------------ */

#define BROWSER_MAX_ENTRIES 64
#define BROWSER_VISIBLE 10 // rows visible at 10px per row (8px font + 2px gap)
#define BROWSER_ROW_H 10
#define BROWSER_PATH_ROW_H 12 // slightly taller path row at top

#define THUMBSTICK_UP_THRESHOLD 2600
#define THUMBSTICK_DOWN_THRESHOLD 1500

typedef struct {
    char path[256];
    char names[BROWSER_MAX_ENTRIES][64];
    uint8_t is_dir[BROWSER_MAX_ENTRIES];
    int count;
    int selected;
    int scroll_offset;
} browser_t;

/* ------------------------------------------------------------------ */
/*  Browser helpers                                                    */
/* ------------------------------------------------------------------ */

static void s_load_dir(browser_t *b)
{
    b->count = 0;

    DIR *dir = opendir(b->path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open dir: %s", b->path);
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && b->count < BROWSER_MAX_ENTRIES) {
        // Skip hidden entries
        if (ent->d_name[0] == '.') {
            continue;
        }
        strncpy(b->names[b->count], ent->d_name, sizeof(b->names[0]) - 1);
        b->names[b->count][sizeof(b->names[0]) - 1] = '\0';
        b->is_dir[b->count] = (ent->d_type == DT_DIR) ? 1 : 0;
        b->count++;
    }
    closedir(dir);
}

static void s_render(const browser_t *b)
{
    oled_clear(OLED_BLACK);

    // Row 0: current path in yellow
    char display_path[22]; // 128px / 6px per char ≈ 21 chars
    strncpy(display_path, b->path, sizeof(display_path) - 1);
    display_path[sizeof(display_path) - 1] = '\0';
    oled_print_string(0, 0, display_path, OLED_YELLOW, OLED_BLACK);

    if (b->count == 0) {
        oled_print_string(0, BROWSER_PATH_ROW_H, "(empty)", OLED_WHITE, OLED_BLACK);
        return;
    }

    int y = BROWSER_PATH_ROW_H;
    for (int i = b->scroll_offset; i < b->count && i < b->scroll_offset + BROWSER_VISIBLE; i++) {
        uint16_t fg = OLED_WHITE;
        uint16_t bg = OLED_BLACK;

        if (i == b->selected) {
            fg = OLED_BLACK;
            bg = OLED_WHITE;
            oled_fill_rect(0, y, 128, BROWSER_ROW_H, OLED_WHITE);
        }

        // Prefix directories with '/'
        char label[22];
        if (b->is_dir[i]) {
            snprintf(label, sizeof(label), "/%s", b->names[i]);
        } else {
            strncpy(label, b->names[i], sizeof(label) - 1);
            label[sizeof(label) - 1] = '\0';
        }

        oled_print_string(2, y + 1, label, fg, bg);
        y += BROWSER_ROW_H;
    }
}

static void s_scroll_clamp(browser_t *b)
{
    if (b->selected < b->scroll_offset) {
        b->scroll_offset = b->selected;
    }
    if (b->selected >= b->scroll_offset + BROWSER_VISIBLE) {
        b->scroll_offset = b->selected - BROWSER_VISIBLE + 1;
    }
}

/* ------------------------------------------------------------------ */
/*  Tasks                                                              */
/* ------------------------------------------------------------------ */

void s_button_task(void *pvParameters)
{
    esp_err_t rc = init_button(GPIO_GREEN_BUTTON);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init green button");
        vTaskDelete(NULL);
        return;
    }

    rc = init_button(GPIO_RED_BUTTON);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init red button");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        xSemaphoreTake(s_mutex_input, portMAX_DELAY);
        s_button_green = button_is_pressed(GPIO_GREEN_BUTTON);
        s_button_red = button_is_pressed(GPIO_RED_BUTTON);
        xSemaphoreGive(s_mutex_input);
        vTaskDelay(pdMS_TO_TICKS(20));
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
        uint32_t x = 0;
        uint32_t y = 0;
        rc = thumbstick_get_values(&x, &y);
        if (rc != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get thumbstick values");
            vTaskDelete(NULL);
            return;
        }
        xSemaphoreTake(s_mutex_input, portMAX_DELAY);
        s_thumbstick_x = x;
        s_thumbstick_y = y;
        xSemaphoreGive(s_mutex_input);
        vTaskDelay(pdMS_TO_TICKS(20));
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
    rc = sdcard_write_file(SD_MOUNT_POINT "/timestamp.txt", timestamp);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write timestamp file");
    } else {
        ESP_LOGI(TAG, "Wrote timestamp: %s", timestamp);
    }

    // Card stays mounted — signal the OLED task
    s_sd_ready = true;
    vTaskDelete(NULL);
}

void s_oled_task(void *pvParameters)
{
    esp_err_t rc = oled_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init OLED");
        vTaskDelete(NULL);
        return;
    }

    oled_print_string(0, 0, "Waiting for SD...", OLED_WHITE, OLED_BLACK);

    // Wait for SD card to be ready
    while (!s_sd_ready) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    browser_t browser;
    memset(&browser, 0, sizeof(browser));
    strncpy(browser.path, SD_MOUNT_POINT, sizeof(browser.path) - 1);
    s_load_dir(&browser);
    s_render(&browser);

    int prev_green = 0;
    int prev_red = 0;
    uint32_t prev_y = 2048;

    while (1) {
        xSemaphoreTake(s_mutex_input, portMAX_DELAY);
        int green = s_button_green;
        int red = s_button_red;
        uint32_t stick_y = s_thumbstick_y;
        xSemaphoreGive(s_mutex_input);

        bool changed = false;

        // Thumbstick: move selection (edge-triggered on threshold cross)
        if (stick_y > THUMBSTICK_UP_THRESHOLD && prev_y <= THUMBSTICK_UP_THRESHOLD) {
            if (browser.selected > 0) {
                browser.selected--;
                s_scroll_clamp(&browser);
                changed = true;
            }
        } else if (stick_y < THUMBSTICK_DOWN_THRESHOLD && prev_y >= THUMBSTICK_DOWN_THRESHOLD) {
            if (browser.selected < browser.count - 1) {
                browser.selected++;
                s_scroll_clamp(&browser);
                changed = true;
            }
        }

        // Green button (rising edge): enter directory
        if (green && !prev_green && browser.count > 0) {
            if (browser.is_dir[browser.selected]) {
                char new_path[256];
                snprintf(new_path, sizeof(new_path), "%s/%s", browser.path, browser.names[browser.selected]);
                strncpy(browser.path, new_path, sizeof(browser.path) - 1);
                browser.selected = 0;
                browser.scroll_offset = 0;
                s_load_dir(&browser);
                changed = true;
            }
        }

        // Red button (rising edge): go up one directory
        if (red && !prev_red) {
            // Don't navigate above mount point
            if (strcmp(browser.path, SD_MOUNT_POINT) != 0) {
                char *slash = strrchr(browser.path, '/');
                if (slash && slash != browser.path) {
                    *slash = '\0';
                } else {
                    strncpy(browser.path, SD_MOUNT_POINT, sizeof(browser.path) - 1);
                }
                browser.selected = 0;
                browser.scroll_offset = 0;
                s_load_dir(&browser);
                changed = true;
            }
        }

        if (changed) {
            s_render(&browser);
        }

        prev_green = green;
        prev_red = red;
        prev_y = stick_y;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ------------------------------------------------------------------ */
/*  app_main                                                           */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "SD card reader starting...");

    s_mutex_input = xSemaphoreCreateMutex();
    if (s_mutex_input == NULL) {
        ESP_LOGE(TAG, "Failed to create input mutex");
        return;
    }

    BaseType_t rc;

    rc = xTaskCreate(s_button_task, "button", 4096, NULL, 5, &s_task_handle_button);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        return;
    }

    rc = xTaskCreate(s_thumbstick_task, "thumbstick", 4096, NULL, 5, &s_task_handle_thumbstick);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create thumbstick task");
        return;
    }

    rc = xTaskCreate(s_sd_card_task, "sdcard", 4096, NULL, 5, &s_task_handle_sd_card);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sdcard task");
        return;
    }

    rc = xTaskCreate(s_oled_task, "oled", 8192, NULL, 5, &s_task_handle_oled);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create oled task");
        return;
    }
}
