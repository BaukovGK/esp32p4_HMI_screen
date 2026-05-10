/**
 * @file ui_pipe.c
 * @brief Реализация помощников для трубопроводов мнемосхемы.
 *
 * Анимация потока (flow-march / recycle-march из proto/style.css)
 * реализована через анимацию `transform_translate_x/y` поверх dashed-линии.
 * Когда линия сдвигается на полный period узора (8+8=16 px для active,
 * 10+6=16 px для recycle), визуальный результат идентичен исходному —
 * loop seamless.
 *
 * Для UI_PIPE_ACTIVE рисуется ДВА объекта:
 *   - solid line — постоянная заливка accent-цветом (5 px)
 *   - overlay (полупрозрачный белый dashed) — анимирован translate
 *
 * Для UI_PIPE_RECYCLE достаточно одной dashed-линии — анимируется она сама.
 */
#include "ui_pipe.h"
#include "ui_tokens.h"

/* ─── контекст пайпа в user_data — для cleanup overlay/points ─────── */
typedef struct {
    lv_point_precise_t pts[2];          /* основной line */
    lv_obj_t          *overlay;         /* для UI_PIPE_ACTIVE — overlay; иначе NULL */
    lv_point_precise_t overlay_pts[2];  /* points для overlay */
} pipe_data_t;

static void pipe_data_free_cb(lv_event_t *e)
{
    pipe_data_t *d = lv_obj_get_user_data(lv_event_get_target(e));
    if (!d) return;
    /* Overlay — sibling в parent'е, удаляем явно (LV_EVENT_DELETE приходит
     * только тому объекту, к которому привязан cb, не siblings). */
    if (d->overlay) {
        lv_obj_delete_async(d->overlay);
    }
    lv_free(d);
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

/** Запустить flow-march анимацию на line (горизонтальной или вертикальной).
 * @param obj         объект для анимации
 * @param horizontal  true → translate_x, false → translate_y
 * @param sign        +1 (вперёд) или -1 (обратное направление)
 * @param period_px   полный шаг dash-узора (например 16)
 * @param duration_ms длительность одного цикла */
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

/* ─── вспомогательные ─────────────────────────────────────────────── */

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

/* ─── публичный API ───────────────────────────────────────────────── */

lv_obj_t *ui_pipe_segment(lv_obj_t *parent,
                          int x1, int y1, int x2, int y2,
                          ui_pipe_kind_t kind)
{
    if (!parent) return NULL;

    pipe_data_t *d = lv_malloc_zeroed(sizeof(*d));
    if (!d) return NULL;
    d->pts[0].x = x1; d->pts[0].y = y1;
    d->pts[1].x = x2; d->pts[1].y = y2;

    /* Базовый line. */
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, d->pts, 2);
    lv_obj_set_style_line_width(line, pipe_width(kind), 0);
    lv_obj_set_style_line_color(line, pipe_color(kind), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_user_data(line, d);
    lv_obj_add_event_cb(line, pipe_data_free_cb, LV_EVENT_DELETE, NULL);

    /* Направление потока — определяется порядком точек (x1→x2 или y1→y2). */
    bool horizontal = (y1 == y2);
    int delta = horizontal ? (x2 - x1) : (y2 - y1);
    int sign = (delta > 0) ? 1 : -1;
    if (delta == 0) sign = 1;  /* безопасное значение для нулевого сегмента */

    if (kind == UI_PIPE_ACTIVE) {
        /* Overlay — белая полупрозрачная dashed-линия поверх solid pipe.
         * Полный period dashed = 8+8 = 16 px. */
        d->overlay_pts[0].x = x1; d->overlay_pts[0].y = y1;
        d->overlay_pts[1].x = x2; d->overlay_pts[1].y = y2;

        lv_obj_t *overlay = lv_line_create(parent);
        lv_line_set_points(overlay, d->overlay_pts, 2);
        lv_obj_set_style_line_color(overlay, ((lv_color_t)LV_COLOR_MAKE(0xff, 0xff, 0xff)), 0);
        lv_obj_set_style_line_width(overlay, 3, 0);
        lv_obj_set_style_line_opa(overlay, (LV_OPA_COVER * 70) / 100, 0);
        lv_obj_set_style_line_dash_width(overlay, 8, 0);
        lv_obj_set_style_line_dash_gap(overlay, 8, 0);
        lv_obj_set_style_line_rounded(overlay, false, 0);
        lv_obj_remove_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
        d->overlay = overlay;

        start_flow_anim(overlay, horizontal, sign, 16, 1400);
    } else if (kind == UI_PIPE_RECYCLE) {
        /* Recycle уже сам dashed (10+6 = 16 px). Анимируем сам line. */
        lv_obj_set_style_line_dash_width(line, 10, 0);
        lv_obj_set_style_line_dash_gap(line, 6, 0);
        lv_obj_set_style_line_rounded(line, false, 0);

        start_flow_anim(line, horizontal, sign, 16, 1600);
    }
    /* INACTIVE — без анимации. */

    return line;
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
