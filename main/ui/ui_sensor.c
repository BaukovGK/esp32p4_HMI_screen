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
#include <stdlib.h>   /* strtof */
#include <math.h>

#define SENSOR_R     22
#define SENSOR_DIAM  (SENSOR_R * 2)
#define TAP_R        4

typedef struct {
    lv_obj_t            *root;
    lv_obj_t            *frame;
    lv_obj_t            *divider;
    lv_obj_t            *tag_label;
    lv_obj_t            *value_label;
    lv_obj_t            *tap;
    lv_obj_t            *leader;
    lv_point_precise_t   leader_points[2];   /* для lv_line */
    ui_sensor_state_t    state;
    const char          *tag;                /* указатель на string literal */
    ui_sensor_click_cb_t click_cb;
} ui_sensor_ctx_t;

/* ─── helpers ─────────────────────────────────────────────────────── */

/* Анимация пульса для DANGER state — opacity 100..50% с playback. */
static void sensor_pulse_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (v * LV_OPA_COVER) / 100, 0);
}

static void start_sensor_pulse(lv_obj_t *frame)
{
    lv_anim_delete(frame, sensor_pulse_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, frame);
    lv_anim_set_exec_cb(&a, sensor_pulse_cb);
    lv_anim_set_values(&a, 100, 40);
    lv_anim_set_duration(&a, 750);
    lv_anim_set_playback_duration(&a, 750);   /* 1.5s полный цикл */
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void stop_sensor_pulse(lv_obj_t *frame)
{
    lv_anim_delete(frame, sensor_pulse_cb);
    lv_obj_set_style_opa(frame, LV_OPA_COVER, 0);
}

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
        /* DANGER → пульсация opacity (как proto sc-pulse keyframes).
         * OFFLINE → снижаем opacity до 70% (имитация dashed border). */
        if (state == UI_SENSOR_DANGER) {
            start_sensor_pulse(ctx->frame);
        } else {
            stop_sensor_pulse(ctx->frame);
            lv_obj_set_style_opa(ctx->frame,
                dashed ? LV_OPA_70 : LV_OPA_COVER, 0);
        }
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
    /* НЕ удаляем tap/leader явно! Они — siblings root'а в общем parent'е.
     * Когда parent (например mnemo canvas) удаляется, LVGL итерирует его
     * children в обратном порядке и destructит каждого. Если sensor cb
     * удалит tap/leader (тоже children parent'а) во время этой итерации,
     * loop index'ы становятся stale → use-after-free и crash при клике
     * на любую вкладку, инициирующую удаление мнемосхемы.
     *
     * В нашем коде sensor создаётся ТОЛЬКО внутри mnemo canvas и
     * удаляется ТОЛЬКО вместе с canvas. Так что orphan tap/leader при
     * deleting sensor отдельно — не возникает.
     *
     * Если в будущем потребуется удалять sensor отдельно — использовать
     * lv_obj_delete_async для tap/leader (deferred). */
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
    /* Клики на frame → bubble to root для ui_sensor_set_click_cb. */
    lv_obj_add_flag(frame, LV_OBJ_FLAG_EVENT_BUBBLE);
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
    lv_obj_add_flag(divider, LV_OBJ_FLAG_EVENT_BUBBLE);
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

    /* Запомнить tag для click handler. cfg->tag должен жить дольше виджета
     * (обычно string literal — это так). */
    ctx->tag = cfg->tag;

    /* Привязать ctx к root + cleanup callback. */
    lv_obj_set_user_data(root, ctx);
    lv_obj_add_event_cb(root, sensor_ctx_free_cb, LV_EVENT_DELETE, ctx);

    /* По умолчанию root кликабелен (lv_obj_create задаёт CLICKABLE).
     * Click handler регистрируется отдельным API ui_sensor_set_click_cb. */

    apply_state(ctx, cfg->state);
    return root;
}

/* Внутренний event handler. Читает текущее значение из value_label
 * (parsing first non-whitespace char). */
static void sensor_click_event_cb(lv_event_t *e)
{
    lv_obj_t *sensor = lv_event_get_current_target(e);
    if (!sensor) return;
    ui_sensor_ctx_t *ctx = lv_obj_get_user_data(sensor);
    if (!ctx || !ctx->click_cb) return;

    float value = NAN;
    if (ctx->value_label) {
        const char *txt = lv_label_get_text(ctx->value_label);
        if (txt && txt[0]) {
            char c = txt[0];
            bool numeric = (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
            if (numeric) {
                char *end = NULL;
                float v = strtof(txt, &end);
                if (end != txt) value = v;
            }
        }
    }

    ctx->click_cb(ctx->tag, value);
}

void ui_sensor_set_click_cb(lv_obj_t *sensor, ui_sensor_click_cb_t cb)
{
    if (!sensor) return;
    ui_sensor_ctx_t *ctx = lv_obj_get_user_data(sensor);
    if (!ctx) return;
    ctx->click_cb = cb;
    if (cb) {
        lv_obj_add_event_cb(sensor, sensor_click_event_cb, LV_EVENT_CLICKED, NULL);
    }
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
