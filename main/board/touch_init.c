#ifndef LVGL_LIVE_PREVIEW
/**
 * @file touch_init.c
 * @brief Инициализация ёмкостного сенсорного контроллера GT911 (I2C) и регистрация в LVGL.
 *
 * Модуль выполняет:
 * - Получение общей шины I2C (из board_i2c)
 * - Создание I2C panel IO для связи с тач-контроллером GT911
 * - Настройку параметров сенсорного контроллера (разрешение, GPIO, зеркалирование)
 * - Регистрацию устройства ввода (touch input) в LVGL
 *
 * Тач-контроллер GT911 подключён по I2C:
 *   SDA = GPIO7, SCL = GPIO8, RST = GPIO22, INT = GPIO21
 *
 * Зависимости: board.h (аппаратные константы), board_i2c.h (общая шина I2C),
 *              esp_lcd_touch_gt911 (драйвер), esp_lvgl_port (интеграция с LVGL)
 */
#include "touch_init.h"
#include "board.h"
#include "board_i2c.h"            // Общая шина I2C
#include "esp_lcd_touch_gt911.h"  // Драйвер ёмкостного тач-контроллера GT911
#include "esp_lvgl_port.h"        // Интеграция LVGL с ESP-IDF
#include "driver/gpio.h"          // GPIO configuration
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch"; // Тег для логирования

/**
 * Полная инициализация тач-контроллера GT911 и регистрация в LVGL.
 *
 * Последовательность:
 * 1. Получение общей шины I2C (должна быть инициализирована ранее)
 * 2. Создание I2C panel IO для связи с GT911
 * 3. Настройка параметров тач-контроллера (разрешение, GPIO, swap/mirror, polling mode)
 * 4. Создание хэндла GT911
 * 5. Регистрация устройства ввода (touch) в LVGL
 *
 * @param disp Указатель на LVGL-дисплей, к которому привязывается сенсорный ввод
 * @return Указатель на LVGL input device, или NULL при ошибке
 */
lv_indev_t *touch_init(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Initializing touch controller");

    /* 1. Получение общей шины I2C (инициализирована в board_i2c_init()) */
    i2c_master_bus_handle_t i2c_bus = board_i2c_get_bus();
    if (!i2c_bus) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return NULL;
    }

    /* 2. Создание I2C panel IO для тач-контроллера GT911 */
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_cfg.scl_speed_hz = BOARD_TOUCH_I2C_FREQ_HZ; // Установка частоты I2C: 400 кГц
    esp_err_t err = esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C panel IO create failed: %s", esp_err_to_name(err));
        return NULL;
    }

    /* 3a. Конфигурация INT-пина (GPIO21) в polling-режиме.
     *     INT не используется прерыванием (режим polling), но нога должна быть
     *     в стабильном состоянии (pull-up) — иначе паразитная ёмкость и плавающий
     *     потенциал будут вызывать лишний расход энергии. */
    gpio_reset_pin(BOARD_TOUCH_INT_GPIO);
    gpio_set_direction(BOARD_TOUCH_INT_GPIO, GPIO_MODE_INPUT);
    gpio_pullup_en(BOARD_TOUCH_INT_GPIO);
    ESP_LOGI(TAG, "Touch INT GPIO configured (polling mode, pull-up enabled)");

    /* 3b. Настройка параметров сенсорного контроллера.
     *     int_gpio_num = GPIO_NUM_NC — используем polling mode (LV_INDEV_MODE_TIMER).
     *     Interrupt mode вызывает WDT crash: GT911 держит INT в LOW после инициализации,
     *     что вызывает шторм прерываний на NEGEDGE и блокирует FreeRTOS tick. */
    /* GT911 на этой плате настроен в ландшафтной ориентации:
     * raw_x = 0..1280 (длинная сторона), raw_y = 0..800 (короткая).
     * swap_xy не нужен — оси уже совпадают с LVGL viewport 1280x800. */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BOARD_DISP_H_RES,           // 1280 — ландшафтный X GT911
        .y_max = BOARD_DISP_V_RES,           // 800  — ландшафтный Y GT911
        .rst_gpio_num = BOARD_TOUCH_RST_GPIO, // GPIO22 — аппаратный сброс тач-контроллера
        .int_gpio_num = GPIO_NUM_NC,          // Polling mode — без прерываний (надёжнее)
        .levels = {
            .reset = 0,       // Активный уровень сброса — низкий
            .interrupt = 0,   // Не используется (polling mode)
        },
        .flags = {
            .swap_xy = 0,     // GT911 уже в ландшафтном режиме — swap не нужен
            .mirror_x = 0,    // Без зеркалирования (проверить на железе, если направление неверное)
            .mirror_y = 0,    // Без зеркалирования
        },
    };

    /* 4. Создание хэндла тач-контроллера GT911 */
    esp_lcd_touch_handle_t touch = NULL;
    err = esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &touch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GT911 touch create failed: %s", esp_err_to_name(err));
        return NULL;
    }
    ESP_LOGI(TAG, "GT911 touch controller initialized");

    /* 5. Регистрация сенсорного ввода в LVGL (привязка к указанному дисплею) */
    const lvgl_port_touch_cfg_t touch_lvgl_cfg = {
        .disp = disp,     // Дисплей, к которому привязан тач
        .handle = touch,   // Хэндл тач-контроллера
    };

    lv_indev_t *indev = lvgl_port_add_touch(&touch_lvgl_cfg);
    if (!indev) {
        ESP_LOGE(TAG, "Failed to add touch to LVGL");
        return NULL;
    }

    ESP_LOGI(TAG, "Touch input registered with LVGL");
    return indev;
}
#endif /* !LVGL_LIVE_PREVIEW */
