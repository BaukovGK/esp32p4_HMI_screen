/**
 * @file ui_filter.h
 * @brief Виджет "Фильтр механической очистки" — ромб 40×40 с буквой "Ф".
 *
 * Порт <polygon class="filter-diamond"> + <text class="filter-letter">
 * из proto/index.html. См. UI_SPEC.md §9.7.
 *
 * LVGL 9 не имеет встроенного polygon с rounded corners, поэтому ромб
 * рисуется через rect 40×40 повёрнутый на 45° (transform_rotation = 450).
 * Получается визуальный ромб, ровно как proto/style.css .filter-diamond.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Создание фильтра. Размещение по center_x/center_y — центру ромба.
 * Под ромбом размещается subtitle "Фильтр" (UTF-8) на 18 px ниже.
 *
 * @param parent       родитель (мнемо-канвас)
 * @param cx, cy       координаты центра ромба (физ. пиксели)
 * @param subtitle     текст под ромбом (UTF-8); NULL → не показывать
 * @return             root container
 */
lv_obj_t *ui_filter_create(lv_obj_t *parent, int cx, int cy, const char *subtitle);

#ifdef __cplusplus
}
#endif
