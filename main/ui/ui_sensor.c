/**
 * @file ui_sensor.c
 * @brief Реализация виджета прибора — leader-line, tap-point, sensor-circle.
 *
 * Структура (всё — siblings в parent):
 *   leader (lv_line) от (tap_x, tap_y) до (circle_x, circle_y)
 *   tap (lv_obj 7×7 circle) центром в (tap_x, tap_y)
 *   circle root (lv_obj container, 44×44, центром в circle_x, circle_y)
 *     ├── frame (lv_obj 44×44 circle)
 *     ├── divider (lv_obj 1×44 horizontal line in middle)
 *     ├── tag_label (UI_FONT_XS, верхняя половина)
 *     └── value_label (UI_FONT_SM, нижняя половина)
 *
 * Контекст в user_data sensor-circle root'а. При удалении root'а
 * освобождается контекст И удаляются tap+leader (через user_data
 * хранятся ссылки на них).
 */
#include "ui_sensor.h"
#include "ui_fonts.h"

#define SENSOR_R     22
#define SENSOR_DIAM  (SENSOR_R * 2)
#define TAP_R        4

typedef struct {
    lv_obj_t          *root;
    lv_obj_t          *frame;
    lv_obj_t          *divider;
    lv_obj_t          *tag_label;
    lv_obj_t          *value_label;
    lv_obj_t          *tap;
    lv_obj_t          *leader;
    lv_point_precise_t leader_points[2];   /* для lv_line */
    ui_sensor_state_t  state;
} ui_sensor_ctx_t;

/* ─── helpers ─────────────────────────────────────────────────────── */

static void apply_state(ui_sensor_ctx_t *ctx, ui_sensor_state_t state)
{
    ctx->state = state;

    lv_color_t stroke;
    lv_color_t text;
    bool dashed = false;

    switch (state) {
    case UI_SENSOR_WARN:
        stroke = ui_token_warning();
        text   = ui_token_warning();
        break;
    case UI_SENSOR_DANGER:
        stroke = ui_token_danger();
        text   = ui_token_danger();
        break;
    case UI_SENSOR_OFFLINE:
        stroke = ui_token_border_strong();
        text   = ui_token_text_muted();
        dashed = true;
        break;
    case UI_SENSOR_OK:
    default:
        stroke = ui_token_accent();
        text   = ui_token_text_primary();
        break;
    }

    if (ctx->frame) {
        lv_obj_set_style_border_color(ctx->frame, stroke, 0);
        lv_obj_set_style_border_width(ctx->frame,
            (state == UI_SENSOR_DANGER) ? 3 : 2, 0);
        /* dashed border не поддерживается LVGL напрямую — для offline
         * визуально снижаем opacity границы (компромисс vs proto). */
        lv_obj_set_style_opa(ctx->frame, dashed ? LV_OPA_70 : LV_OPA_COVER, 0);
    }
    if (ctx->divider) {
        lv_obj_set_style_bg_color(ctx->divider, stroke, 0);
    }
    if (ctx->tag_label) {
        lv_obj_set_style_text_color(ctx->tag_label,
            state == UI_SENSOR_OK ? ui_token_text_secondary() : text, 0);
    }
    if (ctx->value_label) {
        lv_obj_set_style_text_color(ctx->value_label, text, 0);
    }
    if (ctx->tap) {
        lv_obj_set_style_bg_color(ctx->tap, stroke, 0);
    }
    if (ctx->leader) {
        lv_obj_set_style_line_color(ctx->leader, stroke, 0);
    }
}

static void sensor_ctx_free_cb(lv_event_t *e)
{
    ui_sensor_ctx_t *ctx = lv_event_get_user_data(e);
    if (!ctx) return;
    /* Удаляем "осиротевшие" tap и leader, которые не дочерние root'у. */
    if (ctx->tap)    lv_obj_delete(ctx->tap);
    if (ctx->leader) lv_obj_delete(ctx->leader);
    lv_free(ctx);
}

/* ─── публичный API ───────────────────────────────────────────────── */

lv_obj_t *ui_sensor_create(lv_obj_t *parent, const ui_sensor_config_t *cfg)
{
    if (!parent || !cfg) return NULL;

    ui_sensor_ctx_t *ctx = lv_malloc_zeroed(sizeof(*ctx));
    if (!ctx) return NULL;

    /* Leader line — отдельный объект lv_line на parent.
     * Создаётся ПЕРВЫМ, чтобы оказаться под tap+circle в z-order. */
    ctx->leader_points[0].x = cfg->tap_x;
    ctx->leader_points[0].y = cfg->tap_y;
    ctx->leader_points[1].x = cfg->circle_x;
    ctx->leader_points[1].y = cfg->circle_y;
    lv_obj_t *leader = lv_line_create(parent);
    lv_line_set_points(leader, ctx->leader_points, 2);
    lv_obj_set_style_line_width(leader, 1, 0);
    lv_obj_set_style_line_rounded(leader, true, 0);
    ctx->leader = leader;

    /* Tap-point: filled circle 7×7 на трубе. */
    lv_obj_t *tap = lv_obj_create(parent);
    lv_obj_set_size(tap, TAP_R * 2 - 1, TAP_R * 2 - 1);
    lv_obj_set_pos(tap, cfg->tap_x - TAP_R + 1, cfg->tap_y - TAP_R + 1);
    lv_obj_set_style_radius(tap, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(tap, 0, 0);
    lv_obj_set_style_bg_opa(tap, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tap, 0, 0);
    lv_obj_remove_flag(tap, LV_OBJ_FLAG_SCROLLABLE);
    ctx->tap = tap;

    /* Sensor-circle root: container 44×44 центром в (circle_x, circle_y). */
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, SENSOR_DIAM, SENSOR_DIAM);
    lv_obj_set_pos(root, cfg->circle_x - SENSOR_R, cfg->circle_y - SENSOR_R);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    ctx->root = root;

    /* Frame — внешний круг (фон карточки + accent border). */
    lv_obj_t *frame = lv_obj_create(root);
    lv_obj_set_size(frame, SENSOR_DIAM, SENSOR_DIAM);
    lv_obj_set_pos(frame, 0, 0);
    lv_obj_set_style_radius(frame, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(frame, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_pad_all(frame, 0, 0);
    lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    ctx->frame = frame;

    /* Divider — горизонтальная линия по центру круга. */
    lv_obj_t *divider = lv_obj_create(root);
    lv_obj_set_size(divider, SENSOR_DIAM - 4, 1);
    lv_obj_set_pos(divider, 2, SENSOR_R);
    lv_obj_set_style_bg_opa(divider, (LV_OPA_COVER * 60) / 100, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_radius(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
    ctx->divider = divider;

    /* Tag (имя прибора) — верхняя половина круга. */
    lv_obj_t *tag = lv_label_create(root);
    lv_label_set_text(tag, cfg->tag ? cfg->tag : "");
    lv_obj_set_style_text_color(tag, ui_token_text_secondary(), 0);
    lv_obj_set_style_text_font(tag, UI_FONT_XS, 0);
    lv_obj_align(tag, LV_ALIGN_TOP_MID, 0, 6);
    ctx->tag_label = tag;

    /* Value (текущее значение) — нижняя половина круга. */
    lv_obj_t *val = lv_label_create(root);
    lv_label_set_text(val, cfg->value ? cfg->value : "—");
    lv_obj_set_style_text_color(val, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(val, UI_FONT_SM, 0);
    lv_obj_align(val, LV_ALIGN_BOTTOM_MID, 0, -4);
    ctx->value_label = val;

    /* Привязать ctx к root + cleanup callback. */
    lv_obj_set_user_data(root, ctx);
    lv_obj_add_event_cb(root, sensor_ctx_free_cb, LV_EVENT_DELETE, ctx);

    apply_state(ctx, cfg->state);
    return root;
}

void ui_sensor_set_value(lv_obj_t *sensor, const char *value)
{
    if (!sensor || !value) return;
    ui_sensor_ctx_t *ctx = lv_obj_get_user_data(sensor);
    if (!ctx || !ctx->value_label) return;
    lv_label_set_text(ctx->value_label, value);
}

void ui_sensor_set_state(lv_obj_t *sensor, ui_sensor_state_t state)
{
    if (!sensor) return;
    ui_sensor_ctx_t *ctx = lv_obj_get_user_data(sensor);
    if (!ctx) return;
    apply_state(ctx, state);
}
