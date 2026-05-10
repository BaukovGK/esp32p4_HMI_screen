/**
 * @file ui_sensor_modal.h
 * @brief Подробный модал по клику на sensor circle (ISA-5.1).
 *
 * Порт `showSensorDetail(tag)` из proto/app.js. Использует ui_modal
 * для оверлей-структуры. sensorMeta из JS переехала в C-таблицу
 * с порогами warn/alarm, единицами, modbus-источниками.
 *
 * Содержимое модала:
 *   Header:    "P3 — Давление после насоса 1-й ступени"
 *               sub: "между насосом 1-й ст. и мембраной 1-й ст."
 *   Section 1: ТЕКУЩЕЕ ЗНАЧЕНИЕ — большое значение + range bar
 *   Section 2: УСТАВКИ — норма / предупреждение / авария
 *   Section 3: ИСТОЧНИК — Modbus / Обновлено
 *
 * Кнопка "Закрыть" в footer и × в header (см. ui_modal).
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Открыть модал с информацией о датчике.
 *
 * @param tag    "P1" / "Q3" / "σ2" / ... — ASCII-имя как в sensor circle.
 *               Поиск в SENSOR_META по этому tag.
 * @param value  Текущее значение (для отображения и расчёта pct позиции).
 *               NAN → "—", state = offline.
 */
void ui_sensor_modal_show(const char *tag, float value);

#ifdef __cplusplus
}
#endif
