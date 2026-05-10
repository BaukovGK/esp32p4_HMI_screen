/**
 * @file ui_pipe.c
 * @brief Реализация помощников для трубопроводов мнемосхемы.
 */
#include "ui_pipe.h"
#include "ui_tokens.h"

/* Освобождение массива точек, выделенного для lv_line. */
static void pipe_points_free_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    void *pts = lv_obj_get_user_data(target);
    if (pts) lv_free(pts);
}

/* Толщина трубы по типу (см. proto/style.css). */
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

lv_obj_t *ui_pipe_segment(lv_obj_t *parent,
                          int x1, int y1, int x2, int y2,
                          ui_pipe_kind_t kind)
{
    if (!parent) return NULL;

    /* lv_line хранит точки по указателю — выделяем их вместе с line'ом
     * через user_data, чтобы освободились при удалении. */
    lv_point_precise_t *pts = lv_malloc(sizeof(lv_point_precise_t) * 2);
    if (!pts) return NULL;
    pts[0].x = x1; pts[0].y = y1;
    pts[1].x = x2; pts[1].y = y2;

    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, pts, 2);
    lv_obj_set_style_line_width(line, pipe_width(kind), 0);
    lv_obj_set_style_line_color(line, pipe_color(kind), 0);
    lv_obj_set_style_line_rounded(line, true, 0);

    /* Recycle — dashed-линия через нативную LVGL поддержку (line_dash_*).
     * dash 8px / gap 6px ≈ proto stroke-dasharray "6 4". */
    if (kind == UI_PIPE_RECYCLE) {
        lv_obj_set_style_line_dash_width(line, 10, 0);
        lv_obj_set_style_line_dash_gap(line, 6, 0);
        lv_obj_set_style_line_rounded(line, false, 0);  /* острые концы dash'ей */
    }

    /* Сохранить points указатель для авто-освобождения через event_cb. */
    lv_obj_set_user_data(line, pts);
    lv_obj_add_event_cb(line, pipe_points_free_cb, LV_EVENT_DELETE, NULL);

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
