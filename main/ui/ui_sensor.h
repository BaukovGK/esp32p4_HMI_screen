/**
 * @file ui_sensor.h
 * @brief Виджет "Прибор" — ISA-5.1 sensor circle с leader-line и tap-point.
 *
 * Порт <g class="sensor-group"> из proto/index.html. Состав:
 *   - leader (тонкая линия от tap-point на трубе к sensor-circle)
 *   - tap-point (filled circle 3.5px на трубе)
 *   - sensor-circle (круг r=22 с tag сверху, divider, value снизу)
 *
 * Состояния (по порогам warn/alarm — см. UI_SPEC.md §10.2):
 *   ok      — accent stroke (норма)
 *   warn    — warning yellow stroke (предупреждение)
 *   danger  — danger red stroke + pulsation (авария)
 *   offline — dashed border + muted color (нет данных)
 */
#pragma once

#include "lvgl.h"
#include "ui_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SENSOR_OK      = 0,
    UI_SENSOR_WARN    = 1,
    UI_SENSOR_DANGER  = 2,
    UI_SENSOR_OFFLINE = 3,
} ui_sensor_state_t;

/** Конфигурация прибора. */
typedef struct {
    int         tap_x;       /* X точки врезки на трубе (физ. пиксели) */
    int         tap_y;       /* Y точки врезки */
    int         circle_x;    /* X центра sensor-circle (физ. пиксели) */
    int         circle_y;    /* Y центра sensor-circle */
    const char *tag;         /* "P1", "Q3", "σ2"... (UTF-8) */
    const char *value;       /* "3.2", "26.8"... (UTF-8) */
    ui_sensor_state_t state;
} ui_sensor_config_t;

/**
 * Создать sensor-circle с leader-line и tap-point.
 * Все три элемента — siblings в parent (НЕ container), потому что
 * leader-line идёт от tap_x/tap_y до circle_x/circle_y и не должен
 * быть ограничен bbox'ом отдельного контейнера.
 *
 * @param parent  родитель (мнемо-канвас)
 * @param cfg     конфигурация (копируется)
 * @return        указатель на sensor-circle (НЕ leader/tap) — для
 *                последующих ui_sensor_set_* апдейтов; tap и leader
 *                удалятся вместе с ним через event_cb.
 */
lv_obj_t *ui_sensor_create(lv_obj_t *parent, const ui_sensor_config_t *cfg);

/** Обновить значение (текст). */
void ui_sensor_set_value(lv_obj_t *sensor, const char *value);

/** Обновить состояние (цвет всего sensor-group). */
void ui_sensor_set_state(lv_obj_t *sensor, ui_sensor_state_t state);

#ifdef __cplusplus
}
#endif
