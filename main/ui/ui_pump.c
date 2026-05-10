/**
 * @file ui_pump.c
 * @brief Реализация виджета "Насос" — circle + rotating impeller.
 *
 * LVGL-структура:
 *   root (lv_obj container) at (cx-26, cy-26), 52×52
 *   ├── shell (lv_obj, circle, full size) — корпус с цветом по состоянию
 *   ├── rotor (lv_obj container, 36×36, центрирован в shell)
 *   │   ├── blade_h (lv_obj, тонкий горизонтальный rect)
 *   │   ├── blade_v (lv_obj, тонкий вертикальный rect)
 *   │   └── hub (lv_obj, маленький круг в центре)
 *   └── label (lv_label) под кругом
 *
 * Анимации:
 *   - rotor: transform_angle 0..3600 (0..360°) — длительность зависит от state
 *   - shell: opacity pulse 100..50% — для starting/error
 *
 * Контекст в user_data root'а; анимации удаляются при смене state и
 * автоматически при удалении объекта.
 */
#include "ui_pump.h"
#include "ui_fonts.h"
#include <stdlib.h>

#define PUMP_R          26
#define PUMP_DIAM       (PUMP_R * 2)
#define ROTOR_SIZE      36
#define BLADE_LEN       30
#define BLADE_THICK     4
#define HUB_SIZE        8
#define LABEL_OFFSET_Y  44      /* от центра вниз */

typedef struct {
    lv_obj_t       *root;
    lv_obj_t       *shell;
    lv_obj_t       *rotor;
    lv_obj_t       *blade_h;
    lv_obj_t       *blade_v;
    lv_obj_t       *hub;
    lv_obj_t       *label;
    ui_pump_state_t state;
} ui_pump_ctx_t;

/* ─── анимация ────────────────────────────────────────────────────── */

static void pump_rotation_cb(void *obj, int32_t v)
{
    /* v ∈ [0..3600], LVGL уголь измеряется в 0.1° */
    lv_obj_set_style_transform_rotation((lv_obj_t *)obj, v, 0);
}

static void pump_pulse_cb(void *obj, int32_t v)
{
    /* v ∈ [50..100] — opacity в %, маппим в LV_OPA_*. */
    lv_obj_set_style_opa((lv_obj_t *)obj, (v * LV_OPA_COVER) / 100, 0);
}

/* Запустить вращение ротора с заданным периодом, сбросив предыдущую. */
static void start_rotation(lv_obj_t *rotor, uint32_t period_ms)
{
    lv_anim_delete(rotor, pump_rotation_cb);

    /* pivot — центр ротора */
    lv_obj_set_style_transform_pivot_x(rotor, ROTOR_SIZE / 2, 0);
    lv_obj_set_style_transform_pivot_y(rotor, ROTOR_SIZE / 2, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, rotor);
    lv_anim_set_exec_cb(&a, pump_rotation_cb);
    lv_anim_set_values(&a, 0, 3600);
    lv_anim_set_duration(&a, period_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void stop_rotation(lv_obj_t *rotor)
{
    lv_anim_delete(rotor, pump_rotation_cb);
    lv_obj_set_style_transform_rotation(rotor, 0, 0);
}

/* Запустить пульсацию opacity для shell. period_ms = полупериод. */
static void start_pulse(lv_obj_t *shell, uint32_t halfperiod_ms)
{
    lv_anim_delete(shell, pump_pulse_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, shell);
    lv_anim_set_exec_cb(&a, pump_pulse_cb);
    lv_anim_set_values(&a, 100, 50);
    lv_anim_set_duration(&a, halfperiod_ms);
    lv_anim_set_playback_duration(&a, halfperiod_ms);  /* туда-обратно */
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void stop_pulse(lv_obj_t *shell)
{
    lv_anim_delete(shell, pump_pulse_cb);
    lv_obj_set_style_opa(shell, LV_OPA_COVER, 0);
}

/* ─── применить визуал по состоянию ────────────────────────────────── */

static void apply_state(ui_pump_ctx_t *ctx, ui_pump_state_t state)
{
    ctx->state = state;

    /* Цвета корпуса. */
    lv_obj_set_style_bg_color(ctx->shell, ui_token_pump_bg(state), 0);
    lv_obj_set_style_border_color(ctx->shell, ui_token_pump_stroke(state), 0);

    /* Цвета лопастей и hub'а. */
    lv_color_t blade_col = ui_token_pump_blade(state);
    lv_color_t stroke_col = ui_token_pump_stroke(state);
    lv_obj_set_style_bg_color(ctx->blade_h, blade_col, 0);
    lv_obj_set_style_bg_color(ctx->blade_v, blade_col, 0);
    lv_obj_set_style_bg_color(ctx->hub, stroke_col, 0);
    lv_obj_set_style_border_color(ctx->hub, stroke_col, 0);

    /* Управление анимациями. */
    switch (state) {
    case UI_PUMP_RUNNING:
        start_rotation(ctx->rotor, 1500);   /* 1.5s — proto pump-spin */
        stop_pulse(ctx->shell);
        break;
    case UI_PUMP_STARTING:
        start_rotation(ctx->rotor, 3000);   /* 3.0s медленно */
        start_pulse(ctx->shell, 600);       /* 1.2s полный цикл */
        break;
    case UI_PUMP_ERROR:
        stop_rotation(ctx->rotor);
        start_pulse(ctx->shell, 500);       /* 1.0s полный цикл */
        break;
    case UI_PUMP_OFF:
    default:
        stop_rotation(ctx->rotor);
        stop_pulse(ctx->shell);
        break;
    }
}

/* ─── ctx free ─────────────────────────────────────────────────────── */

static void pump_ctx_free_cb(lv_event_t *e)
{
    ui_pump_ctx_t *ctx = lv_event_get_user_data(e);
    if (!ctx) return;
    /* Анимации удаляются автоматически вместе с объектами LVGL,
     * поэтому здесь только освобождение памяти. */
    lv_free(ctx);
}

/* ─── публичный API ───────────────────────────────────────────────── */

lv_obj_t *ui_pump_create(lv_obj_t *parent, const ui_pump_config_t *cfg)
{
    if (!parent || !cfg) return NULL;

    ui_pump_ctx_t *ctx = lv_malloc_zeroed(sizeof(*ctx));
    if (!ctx) return NULL;

    /* root container — 52×64 (диаметр + место для label). Размещён по
     * (cx-26, cy-26) так, чтобы геометрический центр соответствовал
     * (cx, cy) из конфигурации (= центр насоса в прототипе). */
    int root_w = PUMP_DIAM;
    int root_h = PUMP_DIAM + 24;   /* + место под label */
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, root_w, root_h);
    lv_obj_set_pos(root, cfg->cx - PUMP_R, cfg->cy - PUMP_R);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    ctx->root = root;
    lv_obj_set_user_data(root, ctx);
    lv_obj_add_event_cb(root, pump_ctx_free_cb, LV_EVENT_DELETE, ctx);

    /* shell — circle 52×52, размещён в верхней части root'а. */
    lv_obj_t *shell = lv_obj_create(root);
    lv_obj_set_size(shell, PUMP_DIAM, PUMP_DIAM);
    lv_obj_set_pos(shell, 0, 0);
    lv_obj_set_style_radius(shell, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(shell, 2, 0);
    lv_obj_set_style_pad_all(shell, 0, 0);
    lv_obj_set_style_bg_opa(shell, LV_OPA_COVER, 0);
    lv_obj_remove_flag(shell, LV_OBJ_FLAG_SCROLLABLE);
    /* Клики на shell должны передаваться root'у (для attach_equipment_click).
     * По умолчанию lv_obj_create-children CLICKABLE, потребляют клик. */
    lv_obj_add_flag(shell, LV_OBJ_FLAG_EVENT_BUBBLE);
    ctx->shell = shell;

    /* rotor — container 36×36 в центре shell'а. Дочерние лопасти/хаб
     * рисуются от его центра; rotation pivot тоже в его центре. */
    lv_obj_t *rotor = lv_obj_create(shell);
    lv_obj_set_size(rotor, ROTOR_SIZE, ROTOR_SIZE);
    lv_obj_center(rotor);
    lv_obj_set_style_bg_opa(rotor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rotor, 0, 0);
    lv_obj_set_style_pad_all(rotor, 0, 0);
    lv_obj_remove_flag(rotor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(rotor, LV_OBJ_FLAG_EVENT_BUBBLE);
    ctx->rotor = rotor;

    /* blade_h — горизонтальная лопасть. */
    lv_obj_t *blade_h = lv_obj_create(rotor);
    lv_obj_set_size(blade_h, BLADE_LEN, BLADE_THICK);
    lv_obj_center(blade_h);
    lv_obj_set_style_radius(blade_h, BLADE_THICK / 2, 0);
    lv_obj_set_style_border_width(blade_h, 0, 0);
    lv_obj_set_style_pad_all(blade_h, 0, 0);
    lv_obj_set_style_bg_opa(blade_h, LV_OPA_COVER, 0);
    lv_obj_remove_flag(blade_h, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(blade_h, LV_OBJ_FLAG_EVENT_BUBBLE);
    ctx->blade_h = blade_h;

    /* blade_v — вертикальная лопасть. */
    lv_obj_t *blade_v = lv_obj_create(rotor);
    lv_obj_set_size(blade_v, BLADE_THICK, BLADE_LEN);
    lv_obj_center(blade_v);
    lv_obj_set_style_radius(blade_v, BLADE_THICK / 2, 0);
    lv_obj_set_style_border_width(blade_v, 0, 0);
    lv_obj_set_style_pad_all(blade_v, 0, 0);
    lv_obj_set_style_bg_opa(blade_v, LV_OPA_COVER, 0);
    lv_obj_remove_flag(blade_v, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(blade_v, LV_OBJ_FLAG_EVENT_BUBBLE);
    ctx->blade_v = blade_v;

    /* hub — центральный круг. */
    lv_obj_t *hub = lv_obj_create(rotor);
    lv_obj_set_size(hub, HUB_SIZE, HUB_SIZE);
    lv_obj_center(hub);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(hub, 1, 0);
    lv_obj_set_style_pad_all(hub, 0, 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_remove_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hub, LV_OBJ_FLAG_EVENT_BUBBLE);
    ctx->hub = hub;

    /* label под кругом. */
    if (cfg->label) {
        lv_obj_t *label = lv_label_create(root);
        lv_label_set_text(label, cfg->label);
        lv_obj_set_style_text_color(label, ui_token_text_primary(), 0);
        lv_obj_set_style_text_font(label, UI_FONT_XS, 0);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, PUMP_DIAM + 4);
        ctx->label = label;
    }

    apply_state(ctx, cfg->state);

    return root;
}

void ui_pump_set_state(lv_obj_t *pump, ui_pump_state_t state)
{
    if (!pump) return;
    ui_pump_ctx_t *ctx = lv_obj_get_user_data(pump);
    if (!ctx) return;
    apply_state(ctx, state);
}
