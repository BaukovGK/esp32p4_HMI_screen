/**
 * @file alarm_ring.h
 * @brief Кольцевой буфер аварийных сообщений установки обратного осмоса.
 *
 * Хранит до ALARM_RING_SIZE записей типа alarm_entry_t в кольцевом буфере.
 * Новые записи перезаписывают самые старые при переполнении.
 * Потокобезопасность обеспечивается собственным мьютексом (отдельным от plant_data).
 *
 * Зависимости: plant_data.h (тип alarm_entry_t, alarm_category_t).
 */
#pragma once

#include "plant_data.h"

/** Размер кольцевого буфера (максимум хранимых записей аварий) */
#define ALARM_RING_SIZE 16

/** Инициализация кольцевого буфера: обнуление, сброс указателей, создание мьютекса */
void             alarm_ring_init(void);

/**
 * Добавить запись аварии в кольцевой буфер.
 * Если entry->active == false, сначала ищет и деактивирует существующую
 * запись с таким же кодом, затем добавляет запись в историю.
 */
void             alarm_ring_push(const alarm_entry_t *entry);

/**
 * Получить список активных аварий (от новых к старым).
 * @param out       выходной массив (должен вмещать max_count элементов)
 * @param max_count максимальное количество записей для копирования
 * @return          количество скопированных активных записей
 */
int              alarm_ring_get_active(alarm_entry_t *out, int max_count);

/**
 * Получить полную историю аварий (от новых к старым).
 * @param out       выходной массив
 * @param max_count максимальное количество записей для копирования
 * @return          количество скопированных записей
 */
int              alarm_ring_get_history(alarm_entry_t *out, int max_count);

/**
 * Подсчитать количество активных (не снятых) аварий в буфере.
 * @return количество записей с active == true
 */
int              alarm_ring_active_count(void);

/**
 * Определить наихудшую категорию среди активных аварий.
 * @return наивысшая категория (CRITICAL > ALARM > WARNING > INFO)
 */
alarm_category_t alarm_ring_worst_active(void);
