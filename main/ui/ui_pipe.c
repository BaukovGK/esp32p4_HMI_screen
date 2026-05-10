/**
 * @file ui_pipe.c
 * @brief Реализация помощников для трубопроводов мнемосхемы.
 *
 * Анимация потока (marching dashes) — ключевая идея:
 *
 *   1. Каждый animated сегмент оборачивается в clipping-контейнер
 *      размером с bbox трубы (+ запас на line caps).
 *   2. ВНУТРИ контейнера animated-линия (overlay для ACTIVE, сам
 *      pipe-recycle для RECYCLE) ВЫХОДИТ за пределы контейнера на
 *      ОДИН ПЕРИОД UЗОРА (16 px) с каждой стороны вдоль направления
 *      потока. Невидимая (clipped) часть служит "запасом" из которого
 *      выходят новые штрихи при translate.
 *   3. Animation translate_x/y от 0 до ±16 за period_ms. Поскольку
 *      dash-period = translate-range, frame 0 и frame 16 визуально
 *      идентичны → seamless loop.
 *
 * Без extension (п.2) у animated-линии заканчивается заливка штрихов
 * на конце визимой области, и при translate'е возникает «hole» —
 * именно это давало эффект «линия двигается, штрихи стоят».
 */
#include "ui_pipe.h"
#include "ui_tokens.h"

/* Сколько пикселей "запаса" добавляем к animated-линии с каждой
 * стороны вдоль потока. = period узора (8+8 = 10+6 = 16). */
#define FLOW_PERIOD_PX  16

/* ─── контекст пайпа в user_data ──────────────────────────────────── */
typedef struct {
    lv_point_precise_t pts[2];          /* основной line (для INACTIVE и base UI_PIPE_ACTIVE) */
    lv_point_precise_t overlay_pts[2];  /* для overlay UI_PIPE_ACTIVE (extended) или для RECYCLE-line (extended) */
} pipe_data_t;

static void pipe_data_free_cb(lv_event_t *e)
{
    pipe_data_t *d = lv_obj_get_user_data(lv_event_get_target(e));
    if (d) lv_free(d);
}

/* ─── анимация позиции для marching dashes ────────────────────────
 *
 * Используем lv_obj_set_x/set_y (физическое перемещение объекта),
 * а НЕ lv_obj_set_style_translate_x/y. Причина: на lv_line виджете
 * style_translate в LVGL 9 не всегда корректно перерисовывает
 * dashed-pattern — координаты dash'ей рассчитываются от bbox'а
 * объекта, а translate сдвигает только финальные пиксели, не пересчёт
 * draw_task. В результате линия "плывёт", но штрихи относительно
 * базовой green pipe не маршируют.
 *
 * lv_obj_set_x физически меняет obj->pos.x → obj->coords пересчитывается
 * → bbox dash'ей сдвигается → видим марш в абсолютных координатах. */
static void anim_move_x_cb(void *obj, int32_t v)
{
    lv_obj_set_x((lv_obj_t *)obj, v);
}
static void anim_move_y_cb(void *obj, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)obj, v);
}

static void start_flow_anim(lv_obj_t *obj, int x_step, int y_step,
                            uint32_t duration_ms)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    if (x_step != 0) {
        lv_anim_set_exec_cb(&a, anim_move_x_cb);
        lv_anim_set_values(&a, 0, FLOW_PERIOD_PX * x_step);
    } else {
        lv_anim_set_exec_cb(&a, anim_move_y_cb);
        lv_anim_set_values(&a, 0, FLOW_PERIOD_PX * y_step);
    }
    lv_anim_set_duration(&a, duration_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    /* lv_anim_init() устанавливает path_cb = lv_anim_path_linear по
     * default — линейная скорость нужна для seamless wrap-around. */
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

/* ─── helper: clipping-контейнер ─────────────────────────────────── */

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
    /* Не выставляем OVERFLOW_VISIBLE → default = clip children по bbox. */
    return box;
}

/* ─── публичный API ───────────────────────────────────────────────── */

lv_obj_t *ui_pipe_segment(lv_obj_t *parent,
                          int x1, int y1, int x2, int y2,
                          ui_pipe_kind_t kind)
{
    if (!parent) return NULL;

    int width  = pipe_width(kind);
    int half_w = width / 2 + 1;

    /* Направление потока по компонентам. Поток всегда строго гор. или верт.
     * для нашей мнемосхемы — диагоналей нет. */
    int x_step = (x2 > x1) ? 1 : (x2 < x1) ? -1 : 0;
    int y_step = (y2 > y1) ? 1 : (y2 < y1) ? -1 : 0;

    /* INACTIVE — статика, без контейнера. */
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

    /* Координаты основного line — relative от top-left контейнера. */
    int rx1 = x1 - x_min + half_w;
    int ry1 = y1 - y_min + half_w;
    int rx2 = x2 - x_min + half_w;
    int ry2 = y2 - y_min + half_w;

    /* Координаты EXTENDED линии (выходит на FLOW_PERIOD_PX с каждой
     * стороны в направлении потока). Эта линия будет анимирована —
     * extension'ы вне видимой области служат «резервом» штрихов. */
    int ex1 = rx1 - FLOW_PERIOD_PX * x_step;
    int ey1 = ry1 - FLOW_PERIOD_PX * y_step;
    int ex2 = rx2 + FLOW_PERIOD_PX * x_step;
    int ey2 = ry2 + FLOW_PERIOD_PX * y_step;

    if (kind == UI_PIPE_ACTIVE) {
        /* Base — solid green pipe, без анимации, точные координаты. */
        d->pts[0].x = rx1; d->pts[0].y = ry1;
        d->pts[1].x = rx2; d->pts[1].y = ry2;
        lv_obj_t *base = lv_line_create(box);
        lv_line_set_points(base, d->pts, 2);
        lv_obj_set_style_line_width(base, width, 0);
        lv_obj_set_style_line_color(base, pipe_color(kind), 0);
        lv_obj_set_style_line_rounded(base, true, 0);

        /* Overlay — белый dashed, EXTENDED, анимирован translate. */
        d->overlay_pts[0].x = ex1; d->overlay_pts[0].y = ey1;
        d->overlay_pts[1].x = ex2; d->overlay_pts[1].y = ey2;
        lv_obj_t *overlay = lv_line_create(box);
        lv_line_set_points(overlay, d->overlay_pts, 2);
        lv_obj_set_style_line_color(overlay, ((lv_color_t)LV_COLOR_MAKE(0xff, 0xff, 0xff)), 0);
        lv_obj_set_style_line_width(overlay, 3, 0);
        lv_obj_set_style_line_opa(overlay, (LV_OPA_COVER * 75) / 100, 0);
        lv_obj_set_style_line_dash_width(overlay, 8, 0);
        lv_obj_set_style_line_dash_gap(overlay, 8, 0);
        lv_obj_set_style_line_rounded(overlay, false, 0);
        lv_obj_remove_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

        start_flow_anim(overlay, x_step, y_step, 1400);
    } else {
        /* RECYCLE — основная line сама dashed и EXTENDED, анимирована. */
        d->overlay_pts[0].x = ex1; d->overlay_pts[0].y = ey1;
        d->overlay_pts[1].x = ex2; d->overlay_pts[1].y = ey2;
        lv_obj_t *line = lv_line_create(box);
        lv_line_set_points(line, d->overlay_pts, 2);
        lv_obj_set_style_line_width(line, width, 0);
        lv_obj_set_style_line_color(line, pipe_color(kind), 0);
        lv_obj_set_style_line_dash_width(line, 10, 0);
        lv_obj_set_style_line_dash_gap(line, 6, 0);
        lv_obj_set_style_line_rounded(line, false, 0);

        start_flow_anim(line, x_step, y_step, 1600);
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
