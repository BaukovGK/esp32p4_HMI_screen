#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

/**
 * Initialize Ethernet (internal EMAC) and connect to network.
 */
esp_err_t eth_init(void);

/**
 * Wait until Ethernet gets an IP address.
 * @param timeout_ticks Maximum wait time in FreeRTOS ticks
 * @return ESP_OK if got IP, ESP_ERR_TIMEOUT otherwise
 */
esp_err_t eth_wait_for_ip(TickType_t timeout_ticks);
