/**
 * @file scr_washing.h
 * @brief Экран "Химическая промывка" -- управление 7-фазной промывкой мембран.
 *
 * Шаговый индикатор фаз, отображение текущей/целевой температуры,
 * кнопки СТАРТ / ПОДТВЕРДИТЬ / СТОП.
 * Обновление -- периодическое из plant_data_t (wash_sub, temperature, set_washing).
 */
#pragma once

#include "lvgl.h"
#include "plant_data.h"

/**
 * Создаёт экран управления промывкой.
 * Возвращает корневой контейнер с user_data = washing_widgets_t*.
 */
lv_obj_t *scr_washing_create(lv_obj_t *parent);

/**
 * Обновляет состояние фаз, температуру и видимость кнопок.
 * @param container  корневой контейнер
 * @param data       актуальные данные установки
 */
void      scr_washing_update(lv_obj_t *container, const plant_data_t *data);
