/**
 * @file scr_mnemonic.h
 * @brief Мнемосхема -- главный экран технологического процесса обратного осмоса.
 *
 * Отображает P&ID-схему установки: ёмкости, насосы, мембраны, трубопроводы,
 * значения датчиков (давление, температура, расход, электропроводность),
 * телеметрию (степень извлечения, перепад на фильтре) и кнопки управления.
 *
 * Обновление -- периодическое из plant_data_t (вызов scr_mnemonic_update).
 */
#pragma once

#include "lvgl.h"
#include "plant_data.h"

/**
 * Создаёт экран мнемосхемы внутри родительского контейнера.
 * Возвращает указатель на корневой lv_obj, который содержит
 * user_data = mnemonic_widgets_t* для последующего обновления.
 */
lv_obj_t *scr_mnemonic_create(lv_obj_t *parent);

/**
 * Обновляет все виджеты мнемосхемы по данным plant_data_t.
 * Вызывается периодически из UI-таймера (ui_main).
 * @param container  корневой контейнер, созданный scr_mnemonic_create
 * @param data       актуальный снимок данных установки
 */
void      scr_mnemonic_update(lv_obj_t *container, const plant_data_t *data, uint32_t dirty);
