/**
 * @file ui_equipment_modal.h
 * @brief Подробный модал по клику на оборудование (фильтр / насос / мембрана).
 *
 * Порт `showEquipmentDetail(equipmentId)` из proto/app.js. Equipment-id'ы:
 *   "filter"      — фильтр механической очистки
 *   "pump-pre"    — преднагнетательный насос (RO1)
 *   "pump-st1"    — насос 1-й ступени (RO2)
 *   "pump-st2"    — насос 2-й ступени (RO3)
 *   "membrane-1"  — мембрана 1-й ступени
 *   "membrane-2"  — мембрана 2-й ступени
 *
 * Каждый тип имеет свой render layout (см. ui_equipment_modal.c).
 * Метаданные (equipmentMeta из JS) переехали в C-таблицы.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Открыть модал. id должен совпадать с одним из equipment-id'ов выше. */
void ui_equipment_modal_show(const char *equipment_id);

#ifdef __cplusplus
}
#endif
