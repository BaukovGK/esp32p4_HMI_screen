/**
 * @file eth_init.h
 * @brief Заголовочный файл модуля инициализации Ethernet.
 *
 * Предоставляет API для инициализации проводного Ethernet-соединения
 * на ESP32-P4 (встроенный EMAC + PHY IP101) и ожидания получения IP-адреса.
 * Используется для подключения HMI-дисплея к локальной сети
 * перед запуском MQTT-клиента.
 */
#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

/**
 * Инициализация Ethernet (встроенный EMAC) и подключение к сети.
 *
 * Настраивает внутренний MAC, PHY IP101, создает сетевой интерфейс,
 * регистрирует обработчики событий и запускает Ethernet-драйвер.
 *
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
esp_err_t eth_init(void);

/**
 * Блокирующее ожидание получения IP-адреса по Ethernet.
 *
 * Ожидает, пока DHCP-сервер или статическая конфигурация
 * не назначит IP-адрес Ethernet-интерфейсу.
 *
 * @param timeout_ticks Максимальное время ожидания в тиках FreeRTOS
 * @return ESP_OK если IP получен, ESP_ERR_TIMEOUT при превышении таймаута
 */
esp_err_t eth_wait_for_ip(TickType_t timeout_ticks);
