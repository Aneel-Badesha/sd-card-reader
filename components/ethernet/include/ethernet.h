#pragma once

#include "esp_err.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

// Initialize W5500 Ethernet and bring up TCP/IP stack.
// Requires SPI3 bus to be initialized via sdcard_bus_init() first.
esp_err_t ethernet_init(void);

// Open a file at sd_path and send it over TCP to host_ip:port.
// Uses the simple header protocol:
//   [4 bytes BE: filename length][filename][8 bytes BE: file size][data]
// Waits for 1-byte ACK (0x06) from the server.
esp_err_t ethernet_send_file(const char *sd_path, const char *host_ip, uint16_t port);

// Stop Ethernet and release resources.
esp_err_t ethernet_deinit(void);
