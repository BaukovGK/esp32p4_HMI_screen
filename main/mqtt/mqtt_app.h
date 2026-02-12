/**
 * @file mqtt_app.h
 * @brief Заголовочный файл MQTT-клиента HMI-дисплея.
 *
 * Предоставляет API для управления MQTT-клиентом:
 * - Запуск и остановка клиента
 * - Проверка статуса подключения
 * - Публикация команд управления (режим, насосы, дозатор, нагреватель)
 * - Публикация настроек (давление, дозатор, промывка, таймауты)
 *
 * Зависит от модуля plant_data для типов настроек (settings_*_t).
 */
#pragma once

#ifndef LVGL_LIVE_PREVIEW
#include "esp_err.h"
#else
typedef int esp_err_t;
#define ESP_OK 0
#endif

#include "plant_data.h"
#include <stdbool.h>

/** Инициализация и запуск MQTT-клиента (подключение к брокеру) */
esp_err_t mqtt_client_start(void);

/** Остановка и уничтожение MQTT-клиента */
void      mqtt_client_stop(void);

/** Проверка: подключен ли MQTT-клиент к брокеру */
bool      mqtt_client_is_connected(void);

/* Публикация команд управления установкой */

/** Отправка команды смены режима (например, "AUTO", "IDLE", "MANUAL") */
esp_err_t mqtt_publish_mode_cmd(const char *cmd);

/** Отправка битовой маски управления насосами */
esp_err_t mqtt_publish_pump_mask(uint8_t mask);

/** Отправка команды включения/выключения дозатора */
esp_err_t mqtt_publish_doser_enable(bool enabled);

/** Отправка команды включения/выключения нагревателя */
esp_err_t mqtt_publish_heater(bool on);

/* Публикация настроек (уставок) */

/** Отправка настроек уставок давления */
esp_err_t mqtt_publish_settings_pressure(const settings_pressure_t *s);

/** Отправка настроек дозатора (времена работы и цикла) */
esp_err_t mqtt_publish_settings_doser(const settings_doser_t *s);

/** Отправка настроек химической промывки мембран */
esp_err_t mqtt_publish_settings_washing(const settings_washing_t *s);

/** Отправка настроек таймаутов насосов */
esp_err_t mqtt_publish_settings_timeouts(const settings_timeouts_t *s);
