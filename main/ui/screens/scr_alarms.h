/**
 * @file scr_alarms.h
 * @brief Экран "Аварии" -- активные аварии и журнал истории.
 *
 * Верхняя половина -- прокручиваемый список активных аварий.
 * Нижняя половина -- журнал истории (кольцевой буфер, 16 записей).
 * Кнопка СБРОС АВАРИИ видна только в состоянии FAULT.
 * Обновление -- периодическое; данные берутся из alarm_ring и plant_data_t.state.
 */
#pragma once

#include "lvgl.h"
#include "plant_data.h"

/**
 * Создаёт экран аварий.
 * Возвращает корневой контейнер с user_data = alarms_widgets_t*.
 */
lv_obj_t *scr_alarms_create(lv_obj_t *parent);

/**
 * Обновляет списки активных аварий и историю.
 * Перестраивает дочерние виджеты при каждом вызове (lv_obj_clean).
 * @param container  корневой контейнер
 * @param data       актуальные данные установки (нужно для state и btn_reset)
 */
void      scr_alarms_update(lv_obj_t *container, const plant_data_t *data, uint32_t dirty);
