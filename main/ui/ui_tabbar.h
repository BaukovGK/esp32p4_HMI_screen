/**
 * @file ui_tabbar.h
 * @brief Нижняя tabbar (64px) — порт `.tabbar` из proto/index.html.
 *
 * MVP набор из 4 вкладок (см. UI_SPEC §1):
 *   ⌂ Главная     — мнемосхема (id 0)
 *   ⟲ Промывка    — washing screen (id 1)
 *   ⚙ Настройки   — settings screen (id 2)
 *   ⚒ Отладка     — debug screen (id 3)
 *
 * Каждая вкладка — вертикальный layout: иконка (LV_SYMBOL) над лейблом.
 * Активная вкладка получает accent-цвет текста и accent-полосу сверху.
 *
 * Заменяет устаревшую `nav_bar` (60px, 6 кнопок). Высота — UI_TABBAR_H.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Идентификаторы вкладок (порядок отображения слева направо). */
typedef enum {
    UI_TAB_MAIN     = 0,   /* Главная — мнемосхема */
    UI_TAB_WASHING  = 1,   /* Промывка */
    UI_TAB_SETTINGS = 2,   /* Настройки */
    UI_TAB_DEBUG    = 3,   /* Отладка */
    UI_TAB_COUNT
} ui_tab_id_t;

/** Callback при выборе вкладки. */
typedef void (*ui_tabbar_cb_t)(ui_tab_id_t tab);

/**
 * Создание tabbar. Размещается в нижней части parent на y = bottom.
 *
 * @param parent       родительский объект
 * @param on_select    callback при нажатии на вкладку (может быть NULL)
 * @return             указатель на созданный объект
 */
lv_obj_t *ui_tabbar_create(lv_obj_t *parent, ui_tabbar_cb_t on_select);

/** Установить активную вкладку (визуальная подсветка). */
void ui_tabbar_set_active(lv_obj_t *bar, ui_tab_id_t tab);

#ifdef __cplusplus
}
#endif
