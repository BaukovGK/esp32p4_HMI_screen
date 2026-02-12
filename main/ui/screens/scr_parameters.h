/**
 * @file scr_parameters.h
 * @brief Экран "Параметры" -- таблица всех измеренных и расчётных величин.
 *
 * Секции: давление (P1..P4), температура (T), расход (Q1..Q4),
 * электропроводность (S1..S3), расчётные показатели (filter dP, recovery и т.д.).
 * Обновление -- периодическое из plant_data_t.
 */
#pragma once

#include "lvgl.h"
#include "plant_data.h"

/**
 * Создаёт экран параметров (прокручиваемая таблица).
 * Возвращает корневой контейнер с user_data = param_widgets_t*.
 */
lv_obj_t *scr_parameters_create(lv_obj_t *parent);

/**
 * Обновляет значения в таблице параметров.
 * @param container  корневой контейнер
 * @param data       актуальные данные установки
 */
void      scr_parameters_update(lv_obj_t *container, const plant_data_t *data);
