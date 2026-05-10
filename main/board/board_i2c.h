/**
 * @file board_i2c.h
 * @brief Общая шина I2C для периферии платы (MCU дисплея, тач-контроллер).
 *
 * Шина I2C (I2C1, SDA=GPIO7, SCL=GPIO8, 400 кГц) используется:
 *   - Аудиокодек ES8311 (адрес 0x18)
 *   - MCU дисплея Waveshare (адрес 0x45) — предварительная инициализация перед MIPI DSI
 *   - Тач-контроллер GT911 (адрес 0x5D) — ёмкостный сенсорный ввод
 *
 * Должна быть инициализирована до вызова display_init() и touch_init().
 */
#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/**
 * Инициализация общей шины I2C (I2C1, master, 400 кГц).
 *
 * Безопасно вызывать несколько раз — повторные вызовы возвращают ESP_OK.
 *
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
esp_err_t board_i2c_init(void);

/**
 * Получить handle общей шины I2C.
 *
 * Вызывать после board_i2c_init(). Возвращает NULL, если шина не инициализирована.
 *
 * @return Handle шины I2C, или NULL
 */
i2c_master_bus_handle_t board_i2c_get_bus(void);
