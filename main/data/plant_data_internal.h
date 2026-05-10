/**
 * @file plant_data_internal.h
 * @brief Внутренние (не публичные) функции plant_data, доступные nvs_worker.
 *
 * Эти функции выполняют синхронную (блокирующую) запись в NVS в контексте
 * вызывающей задачи. Прямой вызов из UI-task запрещён — используйте
 * публичные plant_data_save_settings_* (они постят задачу воркеру).
 *
 * Заголовок включается ТОЛЬКО в plant_data.c (где функции определены)
 * и nvs_worker.c (где они вызываются из контекста воркера).
 */
#pragma once

#include "plant_data.h"

/** Синхронная запись текущего set_pressure из s_data в NVS (блокирует ~10..100 мс). */
void plant_data_save_settings_pressure_blocking(void);

/** Синхронная запись текущего set_doser из s_data в NVS. */
void plant_data_save_settings_doser_blocking(void);

/** Синхронная запись текущего set_washing из s_data в NVS. */
void plant_data_save_settings_washing_blocking(void);

/** Синхронная запись текущего set_timeouts из s_data в NVS. */
void plant_data_save_settings_timeouts_blocking(void);
