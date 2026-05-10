/**
 * @file ui_pump.h
 * @brief Виджет "Насос" — центробежный насос (ISA-5.1 P&ID).
 *
 * Порт <g class="pump-circle"> из proto/index.html. Геометрия:
 *   - circle r=26 (52×52 px) — корпус
 *   - rotor (cross-pattern: горизонтальная + вертикальная лопасти, hub)
 *   - label под кругом (y_center + 44)
 *
 * Состояния (см. ui_pump_state_t в ui_tokens.h):
 *   running  — accent зелёный, ротор крутится 1.5s
 *   starting — warning жёлтый, ротор медленно (3s) + пульсация opacity
 *   error    — danger красный, ротор статичен + пульсация opacity
 *   off      — серый, статичен
 *
 * См. docs/UI_SPEC.md §9.5 (state matrix) + UI_TOKENS.md §8.2.1.
 */
#pragma once

#include "lvgl.h"
#include "ui_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Конфигурация насоса. */
typedef struct {
    int             cx;          /* центр X в координатах parent (физ. пиксели) */
    int             cy;          /* центр Y */
    const char     *label;       /* подпись под кругом (UTF-8); NULL → нет */
    ui_pump_state_t state;       /* начальное состояние */
} ui_pump_config_t;

/**
 * Создание насоса. Возвращает указатель на root container, к которому
 * можно применять set_state.
 *
 * @param parent   родитель (мнемо-канвас)
 * @param cfg      конфигурация (копируется)
 * @return         корневой объект насоса
 */
lv_obj_t *ui_pump_create(lv_obj_t *parent, const ui_pump_config_t *cfg);

/** Сменить состояние (цвета корпуса/лопастей + анимация ротора + pulse). */
void ui_pump_set_state(lv_obj_t *pump, ui_pump_state_t state);

#ifdef __cplusplus
}
#endif
