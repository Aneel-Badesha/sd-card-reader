#include "ethernet.h"

#include "esp_eth.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

static const char *TAG = "ethernet";

/* ------------------------------------------------------------------ */
/*  W5500 pin definitions (SPI3 / VSPI — shared with SD card)        */
/* ------------------------------------------------------------------ */

#define W5500_CS_PIN 26
#define W5500_INT_PIN 25
#define W5500_RST_PIN 33

/* ------------------------------------------------------------------ */
/*  Module-level state                                                 */
/* ------------------------------------------------------------------ */

#define ETH_CONNECTED_BIT BIT0
#define ETH_GOT_IP_BIT BIT1

static EventGroupHandle_t s_eth_event_group = NULL;
static esp_netif_t *s_netif = NULL;
static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_netif_glue_handle_t s_eth_glue = NULL;

/* ------------------------------------------------------------------ */
/*  Event handlers                                                     */
/* ------------------------------------------------------------------ */

static void s_eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == ETHERNET_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "Ethernet link up");
        xEventGroupSetBits(s_eth_event_group, ETH_CONNECTED_BIT);
    } else if (event_id == ETHERNET_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "Ethernet link down");
        xEventGroupClearBits(s_eth_event_group, ETH_CONNECTED_BIT | ETH_GOT_IP_BIT);
    }
}

static void s_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_eth_event_group, ETH_GOT_IP_BIT);
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

esp_err_t ethernet_init(void)
{
    esp_err_t rc;

    s_eth_event_group = xEventGroupCreate();
    if (s_eth_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    rc = esp_netif_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(rc));
        return rc;
    }

    rc = esp_event_loop_create_default();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(rc));
        return rc;
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&netif_cfg);
    if (s_netif == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // W5500 MAC config — uses SPI3 (already initialized via sdcard_bus_init)
    spi_device_interface_config_t spi_devcfg = {
        .command_bits = 16,
        .address_bits = 8,
        .mode = 0,
        .clock_speed_hz = 20 * 1000 * 1000, // 20 MHz
        .spics_io_num = W5500_CS_PIN,
        .queue_size = 20,
    };
    eth_w5500_config_t w5500_cfg = ETH_W5500_DEFAULT_CONFIG(SPI3_HOST, &spi_devcfg);
    w5500_cfg.int_gpio_num = W5500_INT_PIN;

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.reset_gpio_num = W5500_RST_PIN;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_cfg, &mac_cfg);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_cfg);

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    rc = esp_eth_driver_install(&eth_cfg, &s_eth_handle);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_driver_install failed: %s", esp_err_to_name(rc));
        return rc;
    }

    s_eth_glue = esp_eth_new_netif_glue(s_eth_handle);
    rc = esp_netif_attach(s_netif, s_eth_glue);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_attach failed: %s", esp_err_to_name(rc));
        return rc;
    }

    rc = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, s_eth_event_handler, NULL);
    if (rc != ESP_OK) {
        return rc;
    }
    rc = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, s_ip_event_handler, NULL);
    if (rc != ESP_OK) {
        return rc;
    }

    rc = esp_eth_start(s_eth_handle);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_start failed: %s", esp_err_to_name(rc));
        return rc;
    }

    // Wait up to 10 seconds for an IP address
    EventBits_t bits = xEventGroupWaitBits(s_eth_event_group, ETH_GOT_IP_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    if (!(bits & ETH_GOT_IP_BIT)) {
        ESP_LOGE(TAG, "Timed out waiting for IP address");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Ethernet initialized");
    return ESP_OK;
}

esp_err_t ethernet_send_file(const char *sd_path, const char *host_ip, uint16_t port)
{
    if (sd_path == NULL || host_ip == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Determine file size
    FILE *f = fopen(sd_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s (errno %d)", sd_path, errno);
        return ESP_FAIL;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    // Extract just the filename from the path (strip leading directories)
    const char *filename = strrchr(sd_path, '/');
    filename = filename ? filename + 1 : sd_path;
    uint32_t name_len = (uint32_t)strlen(filename);

    // Connect to server
    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    inet_pton(AF_INET, host_ip, &dest.sin_addr);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        fclose(f);
        return ESP_FAIL;
    }

    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) {
        ESP_LOGE(TAG, "connect() failed: errno %d", errno);
        close(sock);
        fclose(f);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Connected to %s:%u — sending %s (%ld bytes)", host_ip, port, filename, file_size);

    // Send header: [4 bytes BE: name length][name][8 bytes BE: file size]
    // file_size is a long (32-bit on ESP32); upper 4 bytes are always 0
    uint32_t name_len_be = htonl(name_len);
    send(sock, &name_len_be, 4, 0);
    send(sock, filename, name_len, 0);
    uint32_t size_hi = htonl(0);
    uint32_t size_lo = htonl((uint32_t)file_size);
    send(sock, &size_hi, 4, 0);
    send(sock, &size_lo, 4, 0);

    // Stream file in 4 KB chunks
    uint8_t buf[4096];
    size_t sent_total = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (send(sock, buf, n, 0) < 0) {
            ESP_LOGE(TAG, "send() failed at offset %zu: errno %d", sent_total, errno);
            close(sock);
            fclose(f);
            return ESP_FAIL;
        }
        sent_total += n;
    }
    fclose(f);

    // Wait for ACK byte (0x06)
    uint8_t ack = 0;
    if (recv(sock, &ack, 1, 0) == 1 && ack == 0x06) {
        ESP_LOGI(TAG, "File transfer complete — ACK received");
    } else {
        ESP_LOGW(TAG, "No ACK received after transfer");
    }

    close(sock);
    return ESP_OK;
}

esp_err_t ethernet_deinit(void)
{
    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
        esp_eth_del_netif_glue(s_eth_glue);
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }
    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }
    if (s_eth_event_group) {
        vEventGroupDelete(s_eth_event_group);
        s_eth_event_group = NULL;
    }
    ESP_LOGI(TAG, "Ethernet deinitialized");
    return ESP_OK;
}
