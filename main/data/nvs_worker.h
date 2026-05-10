/**
 * @file nvs_worker.h
 * @brief Низкоприоритетный воркер для асинхронной записи уставок в NVS.
 *
 * Назначение: убрать блокирующие flash-write (10..100 мс) из UI-задачи.
 *
 * Архитектура:
 *   - UI-задача после нажатия "Применить" вызывает plant_data_save_settings_*(),
 *     которая обновляет кэш в s_data и постит задачу в очередь воркера через
 *     nvs_worker_enqueue() — это занимает доли миллисекунды и не блокирует UI.
 *   - Воркер (отдельная FreeRTOS-task с приоритетом IDLE+1) в фоне разбирает
 *     очередь и выполняет реальные nvs_set_blob/nvs_commit.
 *
 * Это решение проблемы Top-11 из docs/CODE_REVIEW.md: UI зависал на ~50 мс
 * после каждого "Применить", потому что flash-write выполнялся синхронно
 * в контексте UI-задачи.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

/** Тип задачи NVS-записи (определяет, какой блок настроек писать). */
typedef enum {
    NVS_TASK_SAVE_PRESSURE = 0, /**< settings_pressure_t */
    NVS_TASK_SAVE_DOSER,        /**< settings_doser_t    */
    NVS_TASK_SAVE_WASHING,      /**< settings_washing_t  */
    NVS_TASK_SAVE_TIMEOUTS,     /**< settings_timeouts_t */
    NVS_TASK_COUNT,
} nvs_task_type_t;

/**
 * Стартует воркер: создаёт очередь и FreeRTOS-задачу.
 * Вызывать ОДИН РАЗ в app_main() после plant_data_init().
 *
 * @return ESP_OK при успехе;
 *         ESP_ERR_NO_MEM если не удалось выделить очередь/стек;
 *         ESP_ERR_INVALID_STATE если воркер уже запущен.
 */
esp_err_t nvs_worker_init(void);

/**
 * Поставить задачу в очередь воркера (неблокирующий вариант).
 * UI-задача возвращается сразу же, реальный flash-write выполнится в фоне.
 *
 * @param type тип задачи (NVS_TASK_SAVE_*)
 * @return ESP_OK задача поставлена;
 *         ESP_ERR_INVALID_ARG некорректный type;
 *         ESP_ERR_INVALID_STATE воркер не запущен (вызвать nvs_worker_init);
 *         ESP_ERR_TIMEOUT очередь переполнена (UI накидывает быстрее, чем
 *                          воркер успевает писать) — задача отброшена.
 */
esp_err_t nvs_worker_enqueue(nvs_task_type_t type);

/**
 * Количество необработанных задач в очереди (для диагностики UI).
 * @return 0 если воркер не запущен или очередь пуста.
 */
uint32_t nvs_worker_pending_count(void);
