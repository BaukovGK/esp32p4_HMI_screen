/*
 * app_main.c -- ESP32-P4 HMI entry point
 *
 * Initialization sequence:
 *   1. NVS flash
 *   2. Ethernet
 *   3. Plant data store
 *   4. Alarm ring buffer
 *   5. Language (Russian)
 *   6. Display (MIPI DSI 1280x800)
 *   7. Touch controller
 *   8. UI framework
 *   9. Wait for Ethernet IP (10 s timeout)
 *  10. Start MQTT client
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "display_init.h"
#include "touch_init.h"
#include "eth_init.h"
#include "plant_data.h"
#include "alarm_ring.h"
#include "lang.h"
#include "ui_main.h"
#include "mqtt_app.h"

static const char *TAG = "app_main";

void app_main(void)
{
    /* 1. NVS Flash (needed by Wi-Fi / Ethernet drivers and settings storage) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS flash initialized");

    /* 2. Ethernet (non-fatal: UI works without network) */
    ret = eth_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet initialized");
    } else {
        ESP_LOGW(TAG, "Ethernet init failed: %s, continuing without network", esp_err_to_name(ret));
    }

    /* 3. Plant data store */
    plant_data_init();
    ESP_LOGI(TAG, "Plant data store initialized");

    /* 4. Alarm ring buffer */
    alarm_ring_init();
    ESP_LOGI(TAG, "Alarm ring initialized");

    /* 5. Language */
    lang_init(LANG_RU);
    ESP_LOGI(TAG, "Language set to RU");

    /* 6. Display */
    lv_display_t *disp = display_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display initialization failed");
        return;
    }
    ESP_LOGI(TAG, "Display initialized (1280x800)");

    /* 7. Touch */
    lv_indev_t *indev = touch_init(disp);
    if (indev == NULL) {
        ESP_LOGW(TAG, "Touch initialization failed, continuing without touch");
    } else {
        ESP_LOGI(TAG, "Touch controller initialized");
    }

    /* 8. UI framework (creates screens, nav bar, alarm bar) */
    ui_init(disp);
    ESP_LOGI(TAG, "UI initialized");

    /* 9. Wait for Ethernet IP address (10 second timeout) */
    ret = eth_wait_for_ip(pdMS_TO_TICKS(10000));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet IP acquired");
    } else {
        ESP_LOGW(TAG, "Ethernet IP not acquired within 10 s, continuing anyway");
    }

    /* 10. Start MQTT client */
    ret = mqtt_client_start();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MQTT client started");
    } else {
        ESP_LOGE(TAG, "MQTT client start failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Initialization complete, entering main loop");
}
