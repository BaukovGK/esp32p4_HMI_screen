#ifndef LVGL_LIVE_PREVIEW
/**
 * @file app_main.c
 * @brief Точка входа прошивки HMI-дисплея установки обратного осмоса (ESP32-P4).
 *
 * Последовательность инициализации:
 *   1. NVS Flash — энергонезависимое хранилище (нужно для драйверов и настроек)
 *   2. Ethernet — проводная сеть для связи с основным контроллером ESP32-S3
 *   3. Хранилище данных установки (plant_data)
 *   4. Кольцевой буфер аварий (alarm_ring)
 *   5. Установка языка интерфейса (русский)
 *   6. Инициализация дисплея MIPI DSI 1280x800 (контроллер JD9365)
 *   7. Инициализация сенсорного контроллера GSL3680
 *   8. Запуск UI-фреймворка (экраны, навигация, панель аварий)
 *   9. Ожидание получения IP-адреса по Ethernet (таймаут 10 с)
 *  10. Запуск MQTT-клиента для обмена данными с основным контроллером
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

// Модуль инициализации дисплея (MIPI DSI + LVGL)
#include "display_init.h"
// Модуль инициализации сенсорной панели (I2C + LVGL)
#include "touch_init.h"
// Модуль инициализации Ethernet-соединения
#include "eth_init.h"
// Хранилище параметров установки обратного осмоса
#include "plant_data.h"
// Кольцевой буфер для хранения истории аварий
#include "alarm_ring.h"
// Модуль интернационализации (i18n)
#include "lang.h"
// Главный модуль пользовательского интерфейса (LVGL-экраны)
#include "ui_main.h"
// MQTT-клиент для связи с основным контроллером
#include "mqtt_app.h"

static const char *TAG = "app_main"; // Тег для логирования через ESP_LOG

/**
 * Главная функция приложения (точка входа FreeRTOS).
 * Выполняет последовательную инициализацию всех подсистем HMI:
 * хранилище, сеть, данные, дисплей, тач, UI, MQTT.
 * После инициализации управление передаётся LVGL-циклу и MQTT-задаче.
 */
void app_main(void)
{
    /* 1. NVS Flash — энергонезависимое хранилище (необходимо для Ethernet-драйверов и настроек) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Если NVS повреждён или обновлён — стираем и инициализируем заново
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS flash initialized");

    /* 2. Ethernet — некритичная ошибка: UI работает и без сети */
    ret = eth_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet initialized");
    } else {
        ESP_LOGW(TAG, "Ethernet init failed: %s, continuing without network", esp_err_to_name(ret));
    }

    /* 3. Инициализация хранилища данных установки (давления, расходы, состояния клапанов и т.д.) */
    plant_data_init();
    ESP_LOGI(TAG, "Plant data store initialized");

    /* 4. Инициализация кольцевого буфера аварий (хранение истории аварийных событий) */
    alarm_ring_init();
    ESP_LOGI(TAG, "Alarm ring initialized");

    /* 5. Установка языка интерфейса — русский */
    lang_init(LANG_RU);
    ESP_LOGI(TAG, "Language set to RU");

    /* 6. Инициализация дисплея 10.1" (MIPI DSI, контроллер JD9365, 1280x800 ландшафтный режим) */
    lv_display_t *disp = display_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display initialization failed");
        return; // Без дисплея работа невозможна — выход
    }
    ESP_LOGI(TAG, "Display initialized (1280x800)");

    /* 7. Инициализация сенсорного контроллера GSL3680 (ёмкостный, I2C) */
    lv_indev_t *indev = touch_init(disp);
    if (indev == NULL) {
        // Тач некритичен — продолжаем без него (для отладки)
        ESP_LOGW(TAG, "Touch initialization failed, continuing without touch");
    } else {
        ESP_LOGI(TAG, "Touch controller initialized");
    }

    /* 8. Запуск UI-фреймворка (создание экранов, навигационной панели, панели аварий) */
    ui_init(disp);
    ESP_LOGI(TAG, "UI initialized");

    /* 9. Ожидание получения IP-адреса по Ethernet (таймаут 10 секунд) */
    ret = eth_wait_for_ip(pdMS_TO_TICKS(10000));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet IP acquired");
    } else {
        // IP не получен — MQTT не будет работать, но UI продолжит отображаться
        ESP_LOGW(TAG, "Ethernet IP not acquired within 10 s, continuing anyway");
    }

    /* 10. Запуск MQTT-клиента (подписка на ro_plant/status/# и ro_plant/alarms) */
    ret = mqtt_client_start();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MQTT client started");
    } else {
        ESP_LOGE(TAG, "MQTT client start failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Initialization complete, entering main loop");
}
#endif /* !LVGL_LIVE_PREVIEW */
