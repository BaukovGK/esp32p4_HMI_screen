/**
 * @file ui_pipe.h
 * @brief Помощники для рисования трубопроводов на мнемосхеме.
 *
 * Порт CSS-классов из proto/style.css:
 *   .pipe          — серая, неактивная (для ручных вентилей, дренажа)
 *   .pipe-active   — accent цвет, толщина 5px, активный поток
 *   .pipe-recycle  — info-цвет dashed (цикл рециркуляции)
 *   .pipe-junction — small filled circle (узел разветвления труб)
 *
 * Анимации (.pipe-flow и .pipe-recycle.flowing) — фаза 7 LVGL_PLAN.
 * Сейчас все трубы статичны.
 *
 * API ориентирован на горизонтальные/вертикальные сегменты (как в
 * прототипе — всё ортогонально). Каждый сегмент — отдельный lv_line.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_PIPE_INACTIVE = 0,   /* серая труба */
    UI_PIPE_ACTIVE   = 1,   /* активная (зелёная), полная ширина */
    UI_PIPE_RECYCLE  = 2,   /* рециркуляция (info-color), dashed */
} ui_pipe_kind_t;

/**
 * Создать сегмент трубы как lv_line с двумя точками.
 * Координаты — физ. пиксели в parent.
 *
 * @param parent  родитель (мнемо-канвас)
 * @param x1,y1   начало
 * @param x2,y2   конец
 * @param kind    тип трубы (см. ui_pipe_kind_t)
 * @return        lv_line объект (для возможного удаления/обновления)
 */
lv_obj_t *ui_pipe_segment(lv_obj_t *parent,
                          int x1, int y1, int x2, int y2,
                          ui_pipe_kind_t kind);

/**
 * Узел разветвления трубопровода — small filled circle на трубе.
 * Используется там, где recycle-pipe или dispenser-pipe врезается в
 * pipe-active. Стандарт ISA-5.1 P&ID: filled circle = junction.
 *
 * @param parent  родитель
 * @param cx,cy   центр узла (физ. пиксели)
 * @return        lv_obj junction
 */
lv_obj_t *ui_pipe_junction(lv_obj_t *parent, int cx, int cy);

#ifdef __cplusplus
}
#endif
