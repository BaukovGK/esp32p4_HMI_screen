/**
 * @file ui_membrane.h
 * @brief Виджет "Мембрана" — rect с подписью и horizontal divider.
 *
 * Порт <rect class="equipment-active"> + <text> + <line> из proto/index.html
 * (membrane 1 и 2). Простой rect с rounded corners, подписью внутри и
 * декоративной горизонтальной линией под подписью (имитирует разделение
 * элементов 4040 в кожухе).
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int         x;        /* top-left X (физ. пиксели) */
    int         y;        /* top-left Y */
    int         w;
    int         h;
    const char *label;    /* текст внутри (UTF-8) */
} ui_membrane_config_t;

lv_obj_t *ui_membrane_create(lv_obj_t *parent, const ui_membrane_config_t *cfg);

#ifdef __cplusplus
}
#endif
