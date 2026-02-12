#ifndef LVGL_LIVE_PREVIEW
/**
 * @file touch_init.c
 * @brief Инициализация ёмкостного сенсорного контроллера GSL3680 (I2C) и регистрация в LVGL.
 *
 * Модуль выполняет:
 * - Инициализацию шины I2C в режиме master (I2C0, 400 кГц)
 * - Создание I2C panel IO для связи с тач-контроллером GSL3680
 * - Настройку параметров сенсорного контроллера (разрешение, GPIO, зеркалирование)
 * - Регистрацию устройства ввода (touch input) в LVGL
 *
 * Тач-контроллер GSL3680 подключён по I2C:
 *   SDA = GPIO7, SCL = GPIO8, RST = GPIO22, INT = GPIO21
 *
 * Зависимости: board.h (аппаратные константы), esp_lcd_touch_gsl3680 (драйвер),
 *              esp_lvgl_port (интеграция с LVGL)
 */
#include "touch_init.h"
#include "board.h"
#include "esp_lcd_touch_gsl3680.h" // Драйвер ёмкостного тач-контроллера GSL3680
#include "esp_lvgl_port.h"         // Интеграция LVGL с ESP-IDF
#include "esp_log.h"

static const char *TAG = "touch"; // Тег для логирования

// Статический хэндл шины I2C (инициализируется один раз, используется для тач-контроллера)
static i2c_master_bus_handle_t s_i2c_bus = NULL;

/**
 * Инициализация шины I2C в режиме master.
 *
 * Создаёт шину I2C0 с внутренними подтяжками (pull-up) и фильтром помех.
 * При повторном вызове возвращает ESP_OK без повторной инициализации.
 *
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
static esp_err_t i2c_init(void)
{
    // Защита от повторной инициализации
    if (s_i2c_bus) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BOARD_TOUCH_I2C_PORT,     // I2C0
        .sda_io_num = BOARD_TOUCH_I2C_SDA,    // GPIO7
        .scl_io_num = BOARD_TOUCH_I2C_SCL,    // GPIO8
        .clk_source = I2C_CLK_SRC_DEFAULT,    // Источник тактирования по умолчанию
        .glitch_ignore_cnt = 7,                // Фильтр помех: игнорировать импульсы < 7 тактов
        .flags = {
            .enable_internal_pullup = true,    // Включить встроенные подтяжки (внешние могут отсутствовать)
        },
    };
    return i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
}

/**
 * Полная инициализация тач-контроллера GSL3680 и регистрация в LVGL.
 *
 * Последовательность:
 * 1. Инициализация шины I2C (master, 400 кГц)
 * 2. Создание I2C panel IO для связи с GSL3680
 * 3. Настройка параметров тач-контроллера (разрешение, GPIO, зеркалирование по X)
 * 4. Создание хэндла GSL3680
 * 5. Регистрация устройства ввода (touch) в LVGL
 *
 * @param disp Указатель на LVGL-дисплей, к которому привязывается сенсорный ввод
 * @return Указатель на LVGL input device, или NULL при ошибке
 */
lv_indev_t *touch_init(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Initializing touch controller");

    /* 1. Инициализация шины I2C (master, 400 кГц) */
    ESP_ERROR_CHECK(i2c_init());

    /* 2. Создание I2C panel IO для тач-контроллера GSL3680 */
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GSL3680_CONFIG();
    tp_io_cfg.scl_speed_hz = BOARD_TOUCH_I2C_FREQ_HZ; // Установка частоты I2C: 400 кГц
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_cfg, &tp_io));

    /* 3. Настройка параметров сенсорного контроллера */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BOARD_LCD_H_RES,           // Максимум по X = 800 (нативное разрешение панели)
        .y_max = BOARD_LCD_V_RES,           // Максимум по Y = 1280 (нативное разрешение панели)
        .rst_gpio_num = BOARD_TOUCH_RST_GPIO, // GPIO22 — аппаратный сброс тач-контроллера
        .int_gpio_num = BOARD_TOUCH_INT_GPIO, // GPIO21 — прерывание от тач-контроллера
        .levels = {
            .reset = 0,       // Активный уровень сброса — низкий
            .interrupt = 0,   // Активный уровень прерывания — низкий
        },
        .flags = {
            .swap_xy = 0,     // Не менять X и Y местами
            .mirror_x = 1,    // Зеркалирование по X (согласование с поворотом дисплея на 270 градусов)
            .mirror_y = 0,    // Зеркалирование по Y не требуется
        },
    };

    /* 4. Создание хэндла тач-контроллера GSL3680 */
    esp_lcd_touch_handle_t touch = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gsl3680(tp_io, &tp_cfg, &touch));
    ESP_LOGI(TAG, "GSL3680 touch controller initialized");

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
