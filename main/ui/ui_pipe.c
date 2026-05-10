/**
 * @file ui_pipe.c
 * @brief Реализация помощников для трубопроводов мнемосхемы.
 *
 * Анимация потока: для UI_PIPE_ACTIVE и UI_PIPE_RECYCLE сегмент
 * оборачивается в clipping-контейнер размером bbox+padding. Внутри
 * контейнера живут line + (для active) animated overlay. Когда
 * overlay/recycle-line translate'ится за пределы контейнера —
 * LVGL автоматически обрезает (default behavior без OVERFLOW_VISIBLE).
 *
 * Так визуально решается проблема "overlay вылезает за концы трубы":
 *   • контейнер имеет точные bbox трубы
 *   • анимация translate сдвигает dashes на полный period (16 px)
 *   • за бортом контейнера ничего не видно
 *
 * Для INACTIVE (статичная труба) контейнер не создаётся — line
 * добавляется напрямую в parent (экономия одного объекта на каждый
 * inactive segment, которых обычно немного — drain + dispenser).
 */
#include "ui_pipe.h"
#include "ui_tokens.h"

#include <stdlib.h>   /* для abs() */

/* ─── контекст анимированного пайпа ────────────────────────────────── */
typedef struct {
    lv_point_precise_t pts[2];          /* основной line points (rel coords) */
    lv_point_precise_t overlay_pts[2];  /* overlay points (только для ACTIVE) */
} pipe_data_t;

static void pipe_data_free_cb(lv_event_t *e)
{
    pipe_data_t *d = lv_obj_get_user_data(lv_event_get_target(e));
    if (d) lv_free(d);
}

/* ─── анимация translate ──────────────────────────────────────────── */

static void anim_translate_x_cb(void *obj, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t *)obj, v, 0);
}
static void anim_translate_y_cb(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t *)obj, v, 0);
}

static void start_flow_anim(lv_obj_t *obj, bool horizontal, int sign,
                            int period_px, uint32_t duration_ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, horizontal ? anim_translate_x_cb : anim_translate_y_cb);
    lv_anim_set_values(&a, 0, period_px * sign);
    lv_anim_set_duration(&a, duration_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

/* ─── параметры по типу трубы ─────────────────────────────────────── */

static int pipe_width(ui_pipe_kind_t k)
{
    switch (k) {
    case UI_PIPE_ACTIVE:  return 5;
    case UI_PIPE_RECYCLE: return 3;
    case UI_PIPE_INACTIVE:
    default:              return 4;
    }
}

static lv_color_t pipe_color(ui_pipe_kind_t k)
{
    switch (k) {
    case UI_PIPE_ACTIVE:  return ui_token_pipe_active();
    case UI_PIPE_RECYCLE: return ui_token_info();
    case UI_PIPE_INACTIVE:
    default:              return ui_token_pipe();
    }
}

/* ─── helper: создать прозрачный контейнер ─────────────────────────── */

static lv_obj_t *make_clipping_box(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_radius(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    /* Не выставляем OVERFLOW_VISIBLE — default = clip children. */
    return box;
}

/* ─── публичный API ───────────────────────────────────────────────── */

lv_obj_t *ui_pipe_segment(lv_obj_t *parent,
                          int x1, int y1, int x2, int y2,
                          ui_pipe_kind_t kind)
{
    if (!parent) return NULL;

    int width   = pipe_width(kind);
    int half_w  = width / 2 + 1;
    bool horizontal = (y1 == y2);
    int delta = horizontal ? (x2 - x1) : (y2 - y1);
    int sign  = (delta >= 0) ? 1 : -1;

    /* INACTIVE: статичная труба, без контейнера. */
    if (kind == UI_PIPE_INACTIVE) {
        pipe_data_t *d = lv_malloc_zeroed(sizeof(*d));
        if (!d) return NULL;
        d->pts[0].x = x1; d->pts[0].y = y1;
        d->pts[1].x = x2; d->pts[1].y = y2;

        lv_obj_t *line = lv_line_create(parent);
        lv_line_set_points(line, d->pts, 2);
        lv_obj_set_style_line_width(line, width, 0);
        lv_obj_set_style_line_color(line, pipe_color(kind), 0);
        lv_obj_set_style_line_rounded(line, true, 0);

        lv_obj_set_user_data(line, d);
        lv_obj_add_event_cb(line, pipe_data_free_cb, LV_EVENT_DELETE, NULL);
        return line;
    }

    /* ACTIVE / RECYCLE: clipping-контейнер по bbox трубы. */
    int x_min = (x1 < x2) ? x1 : x2;
    int y_min = (y1 < y2) ? y1 : y2;
    int x_max = (x1 > x2) ? x1 : x2;
    int y_max = (y1 > y2) ? y1 : y2;

    lv_obj_t *box = make_clipping_box(parent,
        x_min - half_w, y_min - half_w,
        (x_max - x_min) + 2 * half_w,
        (y_max - y_min) + 2 * half_w);

    pipe_data_t *d = lv_malloc_zeroed(sizeof(*d));
    if (!d) return box;

    /* Координаты внутри box — relative от его top-left. */
    int rx1 = x1 - x_min + half_w;
    int ry1 = y1 - y_min + half_w;
    int rx2 = x2 - x_min + half_w;
    int ry2 = y2 - y_min + half_w;
    d->pts[0].x = rx1; d->pts[0].y = ry1;
    d->pts[1].x = rx2; d->pts[1].y = ry2;

    /* Базовый line — внутри контейнера. */
    lv_obj_t *line = lv_line_create(box);
    lv_line_set_points(line, d->pts, 2);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, pipe_color(kind), 0);
    lv_obj_set_style_line_rounded(line, true, 0);

    if (kind == UI_PIPE_ACTIVE) {
        /* Overlay — белая dashed-линия 8+8=16 поверх solid pipe.
         * Анимируется translate. Выходящая за концы box часть
         * автоматически клипуется. */
        d->overlay_pts[0].x = rx1; d->overlay_pts[0].y = ry1;
        d->overlay_pts[1].x = rx2; d->overlay_pts[1].y = ry2;

        lv_obj_t *overlay = lv_line_create(box);
        lv_line_set_points(overlay, d->overlay_pts, 2);
        lv_obj_set_style_line_color(overlay, ((lv_color_t)LV_COLOR_MAKE(0xff, 0xff, 0xff)), 0);
        lv_obj_set_style_line_width(overlay, 3, 0);
        lv_obj_set_style_line_opa(overlay, (LV_OPA_COVER * 75) / 100, 0);
        lv_obj_set_style_line_dash_width(overlay, 8, 0);
        lv_obj_set_style_line_dash_gap(overlay, 8, 0);
        lv_obj_set_style_line_rounded(overlay, false, 0);
        lv_obj_remove_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

        start_flow_anim(overlay, horizontal, sign, 16, 1400);
    } else {
        /* RECYCLE — сам line dashed (10+6 = 16). Анимируется он сам. */
        lv_obj_set_style_line_dash_width(line, 10, 0);
        lv_obj_set_style_line_dash_gap(line, 6, 0);
        lv_obj_set_style_line_rounded(line, false, 0);

        start_flow_anim(line, horizontal, sign, 16, 1600);
    }

    lv_obj_set_user_data(box, d);
    lv_obj_add_event_cb(box, pipe_data_free_cb, LV_EVENT_DELETE, NULL);
    return box;
}

lv_obj_t *ui_pipe_junction(lv_obj_t *parent, int cx, int cy)
{
    if (!parent) return NULL;
    /* 8×8 filled circle, цвет text-secondary (как .pipe-junction в proto). */
    lv_obj_t *j = lv_obj_create(parent);
    lv_obj_set_size(j, 8, 8);
    lv_obj_set_pos(j, cx - 4, cy - 4);
    lv_obj_set_style_radius(j, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(j, ui_token_text_secondary(), 0);
    lv_obj_set_style_bg_opa(j, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(j, 0, 0);
    lv_obj_set_style_pad_all(j, 0, 0);
    lv_obj_remove_flag(j, LV_OBJ_FLAG_SCROLLABLE);
    return j;
}
