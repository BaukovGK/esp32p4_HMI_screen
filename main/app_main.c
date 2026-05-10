#ifndef LVGL_LIVE_PREVIEW
/**
 * @file app_main.c
 * @brief Точка входа прошивки HMI-дисплея установки обратного осмоса (ESP32-P4).
 *
 * Последовательность инициализации:
 *   1. NVS Flash — энергонезависимое хранилище (нужно для драйверов и настроек)
 *   2. Инициализация общей шины I2C (для MCU дисплея и тач-контроллера)
 *   3. Инициализация дисплея MIPI DSI 1280x800 (контроллер JD9365, аппаратный сброс GPIO 27)
 *   4. Инициализация сенсорного контроллера GT911
 *   5. Хранилище данных установки (plant_data)
 *   6. Кольцевой буфер аварий (alarm_ring)
 *   7. Установка языка интерфейса (русский)
 *   8. Запуск UI-фреймворка (экраны, навигация, панель аварий)
 *   9. Ethernet — проводная сеть для связи с основным контроллером ESP32-S3
 *  10. Ожидание получения IP-адреса по Ethernet (таймаут 10 с)
 *  11. Запуск MQTT-клиента для обмена данными с основным контроллером
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"

// Общая шина I2C для периферии платы (MCU дисплея, тач-контроллер)
#include "board_i2c.h"
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

#define ETH_WAIT_TIMEOUT_MS  10000

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

    /* 2. Инициализация I2C шины ДО дисплея (как в Waveshare BSP).
     *    Компонент waveshare/esp_lcd_jd9365_10_1 внутри вызывает i2c_bus_create(),
     *    который через i2c_master_get_bus_handle() обнаружит уже существующую шину
     *    и переиспользует её вместо создания новой. */
    ESP_ERROR_CHECK(board_i2c_init());
    ESP_LOGI(TAG, "I2C bus initialized");

    /* 3. Инициализация дисплея 10.1" (MIPI DSI, контроллер JD9365, 1280x800)
     *    I2C MCU pre-init (0x45) выполняется внутри компонента — переиспользует нашу шину. */
    lv_display_t *disp = display_init();
    if (disp == NULL) {
        // Без дисплея HMI бесполезен: оператор не видит состояние установки.
        // Возврат из app_main оставил бы FreeRTOS со «зомби» idle-task без UI и MQTT —
        // оператор не понимал бы что произошло. Перезапуск даёт шанс на восстановление
        // (например, при кратковременном сбое питания LDO или DSI PHY).
        ESP_LOGE(TAG, "display_init failed - restarting in 3s");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }
    ESP_LOGI(TAG, "Display initialized (1280x800)");

    /* 4. Инициализация сенсорного контроллера GT911 (ёмкостный, I2C) */
    lv_indev_t *indev = touch_init(disp);
    if (indev == NULL) {
        // Тач некритичен — продолжаем без него (для отладки)
        ESP_LOGW(TAG, "Touch initialization failed, continuing without touch");
    } else {
        ESP_LOGI(TAG, "Touch controller initialized");
    }

    /* 5. Инициализация хранилища данных установки (давления, расходы, состояния клапанов и т.д.) */
    plant_data_init();
    ESP_LOGI(TAG, "Plant data store initialized");

    /* 6. Инициализация кольцевого буфера аварий (хранение истории аварийных событий) */
    alarm_ring_init();
    ESP_LOGI(TAG, "Alarm ring initialized");

    /* 7. Модуль i18n: по умолчанию русский, NVS-override загружается внутри lang_init() */
    lang_init(LANG_RU);
    ESP_LOGI(TAG, "Language initialized");

    /* 8. Запуск UI-фреймворка (создание экранов, навигационной панели, панели аварий) */
    ui_init(disp);
    ESP_LOGI(TAG, "UI initialized");

    /* 9. Ethernet — некритичная ошибка: UI работает и без сети */
    ret = eth_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet initialized");
    } else {
        ESP_LOGW(TAG, "Ethernet init failed: %s, continuing without network", esp_err_to_name(ret));
    }

    /* 10. Ожидание получения IP-адреса по Ethernet (таймаут 10 секунд) */
    ret = eth_wait_for_ip(pdMS_TO_TICKS(ETH_WAIT_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet IP acquired");
    } else {
        // IP не получен — MQTT не будет работать, но UI продолжит отображаться
        ESP_LOGW(TAG, "Ethernet IP not acquired within 10 s, continuing anyway");
    }

    /* 11. Запуск MQTT-клиента (подписка на ro_plant/status/# и ro_plant/alarms) */
    ret = mqtt_client_start();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MQTT client started");
    } else {
        ESP_LOGE(TAG, "MQTT client start failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Initialization complete, entering main loop");
}
#endif /* !LVGL_LIVE_PREVIEW */
