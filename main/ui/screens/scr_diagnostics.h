/**
 * @file scr_diagnostics.h
 * @brief Экран "Диагностика" -- системная информация контроллера ESP32-S3.
 *
 * Панели: Система (heap, uptime, MQTT), Modbus-устройства (статус, ошибки),
 * Стеки задач (свободное место в байтах).
 * Обновление -- периодическое из plant_data_t.diagnostics.
 */
#pragma once

#include "lvgl.h"
#include "plant_data.h"

/**
 * Создаёт экран диагностики.
 * Возвращает корневой контейнер с user_data = diag_widgets_t*.
 */
lv_obj_t *scr_diagnostics_create(lv_obj_t *parent);

/**
 * Обновляет диагностические показатели.
 * Критичные значения (мало heap / стека) подсвечиваются красным.
 * @param container  корневой контейнер
 * @param data       актуальные данные установки
 */
void      scr_diagnostics_update(lv_obj_t *container, const plant_data_t *data, uint32_t dirty);
