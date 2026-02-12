/**
 * @file mqtt_parser.h
 * @brief Заголовочный файл парсера входящих MQTT-сообщений.
 *
 * Объявляет функцию маршрутизации и разбора входящих MQTT-сообщений.
 * Парсер извлекает данные из JSON-payload и обновляет общее хранилище
 * plant_data и кольцевой буфер аварий alarm_ring.
 *
 * Вызывается из модуля mqtt_app.c при получении события MQTT_EVENT_DATA.
 */
#pragma once

/**
 * Маршрутизация и парсинг входящего MQTT-сообщения.
 *
 * Определяет тип сообщения по топику, разбирает JSON-payload
 * и обновляет plant_data (потокобезопасно) и alarm_ring.
 *
 * @param topic     Указатель на строку топика (без нуль-терминатора)
 * @param topic_len Длина строки топика
 * @param data      Указатель на JSON-payload (без нуль-терминатора)
 * @param data_len  Длина JSON-payload
 */
void mqtt_handle_incoming(const char *topic, int topic_len,
                          const char *data, int data_len);
