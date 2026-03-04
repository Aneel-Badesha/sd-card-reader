#pragma once

#include "driver/sdspi_host.h"
#include <stdio.h>

void sdmmc_card_print_info(FILE *stream, const sdmmc_card_t *card);
