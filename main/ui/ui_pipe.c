/**
 * @file ui_pipe.c
 * @brief Реализация помощников для трубопроводов мнемосхемы.
 *
 * Анимация потока (marching dashes) — техника из LVGL forum:
 *   https://forum.lvgl.io/t/how-to-achieve-pipeline-flow-by-line/12973
 *
 * Идея:
 *   1. Pipe = clipping-контейнер (lv_obj) фиксированной формы и
 *      размера, БЕЗ собственного движения. Внутри — overlay rect.
 *   2. Overlay rect — ДЛИННЕЕ pipe на 16 px (= период dash-паттерна)
 *      с каждой стороны вдоль направления потока. Имеет:
 *      - bg_color (solid pipe color для ACTIVE, transparent для RECYCLE)
 *      - bg_image = tiled dash pattern (ARGB8888 16×1 для горизонтальной
 *        трубы или 1×16 для вертикальной).
 *      - bg_image_recolor для подкраски паттерна в нужный цвет.
 *   3. Анимация: lv_obj_set_x/y на overlay rect от 0 до ±16. Контейнер
 *      клипует overlay по bbox pipe'а. Тайлы внутри overlay сдвигаются
 *      вместе с ним → визуально дашики маршируют в окне pipe'а.
 *   4. Когда offset = 16 = period, тайлы вернулись в исходное положение
 *      → seamless loop без видимого "снэпа".
 *
 * INACTIVE-трубы остаются как lv_line (статика, дешевле).
 */
#include "ui_pipe.h"
#include "ui_tokens.h"

#define FLOW_PERIOD_PX  16   /* шаг dash-паттерна */

/* ─── dash-tile (image data) ───────────────────────────────────────
 * ARGB8888 byte order в LVGL 9: { B, G, R, A } per pixel.
 * 8 px white opaque + 8 px transparent → period 16 px.
 */

#define WHITE_PX  0xFF, 0xFF, 0xFF, 0xFF
#define TRANS_PX  0x00, 0x00, 0x00, 0x00

/* Горизонтальная — для труб где dashes должны двигаться по X. */
static const uint8_t dash_h_data[64] = {
    WHITE_PX, WHITE_PX, WHITE_PX, WHITE_PX,
    WHITE_PX, WHITE_PX, WHITE_PX, WHITE_PX,
    TRANS_PX, TRANS_PX, TRANS_PX, TRANS_PX,
    TRANS_PX, TRANS_PX, TRANS_PX, TRANS_PX,
};

static const lv_image_dsc_t dash_h_img = {
    .header = {
        .magic  = LV_IMAGE_HEADER_MAGIC,
        .cf     = LV_COLOR_FORMAT_ARGB8888,
        .w      = 16,
        .h      = 1,
    },
    .data_size = sizeof(dash_h_data),
    .data      = dash_h_data,
};

/* Вертикальная — для труб где dashes должны двигаться по Y. */
static const uint8_t dash_v_data[64] = {
    WHITE_PX, WHITE_PX, WHITE_PX, WHITE_PX,
    WHITE_PX, WHITE_PX, WHITE_PX, WHITE_PX,
    TRANS_PX, TRANS_PX, TRANS_PX, TRANS_PX,
    TRANS_PX, TRANS_PX, TRANS_PX, TRANS_PX,
};

static const lv_image_dsc_t dash_v_img = {
    .header = {
        .magic  = LV_IMAGE_HEADER_MAGIC,
        .cf     = LV_COLOR_FORMAT_ARGB8888,
        .w      = 1,
        .h      = 16,
    },
    .data_size = sizeof(dash_v_data),
    .data      = dash_v_data,
};

/* ─── контекст пайпа ──────────────────────────────────────────────── */
typedef struct {
    lv_point_precise_t pts[2];  /* для INACTIVE lv_line */
} pipe_data_t;

static void pipe_data_free_cb(lv_event_t *e)
{
    pipe_data_t *d = lv_obj_get_user_data(lv_event_get_target(e));
    if (d) lv_free(d);
}

/* ─── анимация позиции overlay rect'а ─────────────────────────────── */

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
        /* Overlay extended -16 with offset 0 by default; animate 0..16
         * (для positive flow) или 0..-16 (для negative). */
        lv_anim_set_values(&a, 0, FLOW_PERIOD_PX * x_step);
    } else {
        lv_anim_set_exec_cb(&a, anim_move_y_cb);
        lv_anim_set_values(&a, 0, FLOW_PERIOD_PX * y_step);
    }
    lv_anim_set_duration(&a, duration_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

/* ─── параметры по типу трубы ─────────────────────────────────────── */

static int pipe_thickness(ui_pipe_kind_t k)
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

    int thickness = pipe_thickness(kind);
    int half = thickness / 2;
    bool horizontal = (y1 == y2);
    int x_step = (x2 > x1) ? 1 : (x2 < x1) ? -1 : 0;
    int y_step = (y2 > y1) ? 1 : (y2 < y1) ? -1 : 0;

    /* INACTIVE — статика, lv_line. */
    if (kind == UI_PIPE_INACTIVE) {
        pipe_data_t *d = lv_malloc_zeroed(sizeof(*d));
        if (!d) return NULL;
        d->pts[0].x = x1; d->pts[0].y = y1;
        d->pts[1].x = x2; d->pts[1].y = y2;

        lv_obj_t *line = lv_line_create(parent);
        lv_line_set_points(line, d->pts, 2);
        lv_obj_set_style_line_width(line, thickness, 0);
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

    int box_w, box_h;
    if (horizontal) {
        box_w = (x_max - x_min);
        box_h = thickness;
    } else {
        box_w = thickness;
        box_h = (y_max - y_min);
    }
    int box_x = horizontal ? x_min : (x_min - half);
    int box_y = horizontal ? (y_min - half) : y_min;

    /* Внешний контейнер — клипует overlay по bbox.
     * ВАЖНО: solid pipe-цвет (для ACTIVE) живёт ИМЕННО на box'е, чтобы не
     * двигался вместе с overlay'ем. Иначе при сдвиге overlay'а слева от
     * pipe'а появлялся бы пустой "зазор". */
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_pos(box, box_x, box_y);
    lv_obj_set_size(box, box_w, box_h);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_radius(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);

    if (kind == UI_PIPE_ACTIVE) {
        /* Solid pipe-цвет статичен — нанесён на box. */
        lv_obj_set_style_bg_color(box, pipe_color(kind), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    } else {
        /* RECYCLE — только дашики, подложка прозрачная. */
        lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    }

    /* Overlay rect — расширен на FLOW_PERIOD_PX с каждой стороны вдоль
     * потока. Tiled dash background. Анимируется его позиция. */
    int ov_w, ov_h, ov_x, ov_y;
    if (horizontal) {
        ov_w = box_w + 2 * FLOW_PERIOD_PX;
        ov_h = box_h;
        ov_x = -FLOW_PERIOD_PX;   /* extended left */
        ov_y = 0;
    } else {
        ov_w = box_w;
        ov_h = box_h + 2 * FLOW_PERIOD_PX;
        ov_x = 0;
        ov_y = -FLOW_PERIOD_PX;
    }

    lv_obj_t *overlay = lv_obj_create(box);
    lv_obj_set_pos(overlay, ov_x, ov_y);
    lv_obj_set_size(overlay, ov_w, ov_h);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    /* Overlay только переносит tiled-картинку — собственного bg НЕТ,
     * иначе при движении overlay'а его подложка двигалась бы вместе с
     * ним и создавала видимый "зазор" у начала pipe'а. */
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    /* bg_image = tiled dash pattern. Recolor подкрашивает белый паттерн
     * в нужный цвет (для ACTIVE — белые дашики поверх зелёной box-подложки,
     * для RECYCLE — info-cyan дашики на прозрачном фоне). */
    const lv_image_dsc_t *tile = horizontal ? &dash_h_img : &dash_v_img;
    lv_obj_set_style_bg_image_src(overlay, tile, 0);
    lv_obj_set_style_bg_image_tiled(overlay, true, 0);

    if (kind == UI_PIPE_ACTIVE) {
        /* Белые дашики поверх зелёного box — 75% opa, чтобы зелёный
         * "просвечивал" сквозь дашики и не было резкого контраста. */
        lv_obj_set_style_bg_image_opa(overlay, (LV_OPA_COVER * 75) / 100, 0);
    } else {
        lv_obj_set_style_bg_image_recolor(overlay, pipe_color(kind), 0);
        lv_obj_set_style_bg_image_recolor_opa(overlay, LV_OPA_COVER, 0);
    }

    /* Стартовая позиция overlay'а — 0 (по умолчанию).
     * Анимация двигает её 0 → ±FLOW_PERIOD_PX и обратно (loop). */
    uint32_t duration = (kind == UI_PIPE_ACTIVE) ? 1400 : 1600;
    start_flow_anim(overlay, x_step, y_step, duration);

    return box;
}

lv_obj_t *ui_pipe_junction(lv_obj_t *parent, int cx, int cy)
{
    if (!parent) return NULL;
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
