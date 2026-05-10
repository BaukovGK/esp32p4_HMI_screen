/**
 * @file ui_tank.h
 * @brief Виджет "Ёмкость" для мнемосхемы — порт `<g class="tank-group">` из прототипа.
 *
 * Состав (см. proto/index.html и docs/UI_SPEC.md §9.6):
 *   1. Корпус (rect с rounded corners, clip_corner=true для воды снизу)
 *   2. Вода — child rect с linear-gradient (light cyan → deep blue)
 *   3. Тонкая «поверхность» (highlight 1.5px) на верху воды
 *   4. Название ёмкости — над водой
 *   5. % заполнения — по центру воды
 *   6. Стержень-зонд (вертикальная полоса) с поплавковыми датчиками (DI*)
 *   7. До UI_TANK_MAX_SWITCHES датчиков уровня на стержне
 *
 * Координаты передаются в физических пикселях. Конвертация SVG→px
 * через MNEMO_PX() (см. ui_tokens.h).
 *
 * Анимация волны на поверхности (5s ease-in-out translateX) — TODO,
 * добавится в фазе 7 LVGL_IMPLEMENTATION_PLAN.md.
 */
#pragma once

#include "lvgl.h"
#include "ui_tokens.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Лимит датчиков на одной ёмкости (Промеж имеет 2: DI2+DI3). */
#define UI_TANK_MAX_SWITCHES 4

/** Геометрия и базовое содержимое ёмкости. */
typedef struct {
    int         x;          /* top-left X в координатах parent (физ. пиксели) */
    int         y;          /* top-left Y */
    int         w;          /* ширина */
    int         h;          /* высота */
    int         fill_pct;   /* 0..100 — текущий уровень */
    const char *name;       /* лейбл ёмкости (UTF-8). NULL → не показывать. */
} ui_tank_geom_t;

/** Один поплавковый датчик уровня на стержне. */
typedef struct {
    int                 y_offset;   /* Y относительно tank top (px) */
    const char         *tag;        /* "DI1" / "DI2" / ... */
    ui_level_sw_state_t state;      /* dry / active / alarm */
} ui_tank_sw_t;

/** Полная конфигурация для ui_tank_create. */
typedef struct {
    ui_tank_geom_t geom;
    ui_tank_sw_t   switches[UI_TANK_MAX_SWITCHES];
    int            switch_count;
} ui_tank_config_t;

/**
 * Создание ёмкости в parent. Возвращает указатель на shell — корневой
 * объект ёмкости, к которому можно применять set_fill/set_switch_state.
 *
 * Внутренний контекст (ссылки на дочерние объекты) хранится в
 * user_data shell'а и освобождается автоматически при удалении объекта.
 *
 * @param parent  родительский контейнер (например мнемо-канвас)
 * @param cfg     полная конфигурация — копируется внутрь, можно стек
 * @return        указатель на shell-объект (lv_obj)
 */
lv_obj_t *ui_tank_create(lv_obj_t *parent, const ui_tank_config_t *cfg);

/** Обновить уровень заполнения. pct ∈ [0..100]. */
void ui_tank_set_fill(lv_obj_t *tank, int pct);

/** Обновить состояние конкретного датчика (idx ∈ [0..switch_count-1]). */
void ui_tank_set_switch_state(lv_obj_t *tank, int idx, ui_level_sw_state_t state);

#ifdef __cplusplus
}
#endif
