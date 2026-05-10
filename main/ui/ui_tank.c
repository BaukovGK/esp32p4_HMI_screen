/**
 * @file ui_tank.c
 * @brief Реализация виджета "Ёмкость" — порт <g class="tank-group">.
 *
 * Структура LVGL-объектов:
 *   shell (lv_obj, rect, rounded, clip_corner)
 *   ├── water (lv_obj, rect, gradient bg) — height = h * fill_pct / 100
 *   │   └── surface_hl (lv_obj, тонкая полоса наверху воды)
 *   ├── name_label (lv_label) — поверх воды/воздуха
 *   ├── pct_label (lv_label)
 *   ├── probe (lv_obj, тонкий vertical rect)
 *   ├── sw_dot[N] (lv_obj, circle)
 *   └── sw_tag[N] (lv_label)
 *
 * Контекст (ui_tank_ctx_t) хранится в user_data shell'а — позволяет
 * find children после создания (для set_fill/set_switch_state).
 */
#include "ui_tank.h"
#include "ui_fonts.h"
#include <stdlib.h>
#include <stdio.h>

/* ─── внутренний контекст ─────────────────────────────────────────── */

typedef struct {
    lv_obj_t *shell;
    lv_obj_t *water;
    lv_obj_t *pct_label;
    int       w, h;
    int       sw_count;
    lv_obj_t *sw_dots[UI_TANK_MAX_SWITCHES];
    lv_obj_t *sw_tags[UI_TANK_MAX_SWITCHES];
} ui_tank_ctx_t;

static void tank_ctx_free_cb(lv_event_t *e)
{
    ui_tank_ctx_t *ctx = lv_event_get_user_data(e);
    if (ctx) lv_free(ctx);
}

/* ─── вспомогательные функции для отрисовки ───────────────────────── */

/** Применить геометрию воды по текущему % (water_h = h * pct / 100,
 *  water_top_y = h - water_h). Также пересчитать pct_label. */
static void update_water_geometry(ui_tank_ctx_t *ctx, int pct)
{
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    int water_h = (ctx->h * pct) / 100;
    int water_y = ctx->h - water_h;

    if (water_h < 1) water_h = 1; /* не давать LVGL ругаться на нулевую высоту */

    lv_obj_set_size(ctx->water, ctx->w, water_h);
    lv_obj_set_pos(ctx->water, 0, water_y);

    if (ctx->pct_label) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(ctx->pct_label, buf);
        /* Центр воды по вертикали */
        int label_y = water_y + water_h / 2 - 9; /* ~half font height */
        lv_obj_align(ctx->pct_label, LV_ALIGN_TOP_MID, 0, label_y);
    }
}

/* Линейный градиент (CSS: linear-gradient(to bottom, light, dark)).
 * LVGL 9: bg_color = top stop, bg_grad_color = bottom stop, dir VER. */
static void apply_water_gradient(lv_obj_t *water)
{
    /* Жёстко прописанные цвета воды (matches proto tankWaterGrad
     * + UI_TOKENS.md §8.2.3). Не зависят от light/dark темы — вода
     * всегда голубая. */
    lv_color_t top_stop = ((lv_color_t)LV_COLOR_MAKE(0x7c, 0xc8, 0xf5));
    lv_color_t bot_stop = ((lv_color_t)LV_COLOR_MAKE(0x1f, 0x7c, 0xb8));

    lv_obj_set_style_bg_color(water, top_stop, 0);
    lv_obj_set_style_bg_grad_color(water, bot_stop, 0);
    lv_obj_set_style_bg_grad_dir(water, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(water, (LV_OPA_COVER * 85) / 100, 0);
}

/* Маленькая «таблетка» с датчиком уровня (точка + tag). */
static void create_level_switch(lv_obj_t *parent, ui_tank_ctx_t *ctx,
                                int idx, int probe_x, const ui_tank_sw_t *sw)
{
    /* Точка датчика (8×8 круг). */
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_pos(dot, probe_x - 4, sw->y_offset - 4);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, ui_token_level_sw_fill(sw->state), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dot, ui_token_level_sw_stroke(sw->state), 0);
    lv_obj_set_style_border_width(dot, 1, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    ctx->sw_dots[idx] = dot;

    /* Tag — текст справа от точки. */
    lv_obj_t *tag = lv_label_create(parent);
    lv_label_set_text(tag, sw->tag ? sw->tag : "");
    lv_obj_set_style_text_color(tag, ui_token_level_sw_stroke(sw->state), 0);
    lv_obj_set_style_text_font(tag, UI_FONT_XS, 0);
    lv_obj_set_pos(tag, probe_x + 8, sw->y_offset - 6);
    ctx->sw_tags[idx] = tag;
}

/* ─── публичный API ───────────────────────────────────────────────── */

lv_obj_t *ui_tank_create(lv_obj_t *parent, const ui_tank_config_t *cfg)
{
    if (!parent || !cfg) return NULL;

    ui_tank_ctx_t *ctx = lv_malloc_zeroed(sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->w = cfg->geom.w;
    ctx->h = cfg->geom.h;
    ctx->sw_count = cfg->switch_count > UI_TANK_MAX_SWITCHES
                    ? UI_TANK_MAX_SWITCHES : cfg->switch_count;

    /* 1. Shell — корпус ёмкости. */
    lv_obj_t *shell = lv_obj_create(parent);
    lv_obj_set_size(shell, ctx->w, ctx->h);
    lv_obj_set_pos(shell, cfg->geom.x, cfg->geom.y);
    lv_obj_set_style_radius(shell, UI_RADIUS_MD, 0);
    lv_obj_set_style_bg_color(shell, ui_token_bg_mute(), 0);
    lv_obj_set_style_bg_opa(shell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(shell, ui_token_tank_stroke(), 0);
    lv_obj_set_style_border_width(shell, 2, 0);
    lv_obj_set_style_pad_all(shell, 0, 0);
    /* Обрезка содержимого по rounded углам — вода будет иметь
     * скруглённый низ, ровный верх (cleanly clipped). */
    lv_obj_set_style_clip_corner(shell, true, 0);
    lv_obj_remove_flag(shell, LV_OBJ_FLAG_SCROLLABLE);

    ctx->shell = shell;
    lv_obj_set_user_data(shell, ctx);
    lv_obj_add_event_cb(shell, tank_ctx_free_cb, LV_EVENT_DELETE, ctx);

    /* 2. Вода — child ёмкости с градиентом. */
    lv_obj_t *water = lv_obj_create(shell);
    lv_obj_set_style_radius(water, 0, 0);
    lv_obj_set_style_border_width(water, 0, 0);
    lv_obj_set_style_pad_all(water, 0, 0);
    lv_obj_remove_flag(water, LV_OBJ_FLAG_SCROLLABLE);
    apply_water_gradient(water);
    ctx->water = water;

    /* 3. Surface highlight — тонкая полоса 1.5px на верху воды.
     * Имитирует .tank-wave-line (animated ripple) — пока статичная. */
    lv_obj_t *surface = lv_obj_create(water);
    lv_obj_set_size(surface, cfg->geom.w, 2);
    lv_obj_set_pos(surface, 0, 0);
    lv_obj_set_style_radius(surface, 0, 0);
    lv_obj_set_style_border_width(surface, 0, 0);
    lv_obj_set_style_pad_all(surface, 0, 0);
    lv_obj_set_style_bg_color(surface, ((lv_color_t)LV_COLOR_MAKE(0xff, 0xff, 0xff)), 0);
    lv_obj_set_style_bg_opa(surface, (LV_OPA_COVER * 55) / 100, 0);
    lv_obj_remove_flag(surface, LV_OBJ_FLAG_SCROLLABLE);

    /* 4. Название ёмкости — над водой. */
    if (cfg->geom.name) {
        lv_obj_t *name = lv_label_create(shell);
        lv_label_set_text(name, cfg->geom.name);
        lv_obj_set_style_text_color(name, ui_token_text_primary(), 0);
        lv_obj_set_style_text_font(name, UI_FONT_SM, 0);
        /* Top-center inside air space — y=12px from top fits any tank. */
        lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 12);
    }

    /* 5. Pct label — текст по центру воды. Создаётся даже без воды. */
    lv_obj_t *pct = lv_label_create(shell);
    lv_label_set_text(pct, "0%");
    lv_obj_set_style_text_color(pct, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(pct, UI_FONT_MD, 0);
    /* Halo через outline (не paint-order:stroke в LVGL — используем тень
     * с радиусом 0 для эффекта обводки). Альтернатива — без halo: текст
     * 700 weight на воде должен быть достаточно читаемым. */
    ctx->pct_label = pct;

    /* 6. Стержень-зонд — вертикальная тонкая линия, 16px от левого края. */
    int probe_x = 16;
    lv_obj_t *probe = lv_obj_create(shell);
    lv_obj_set_size(probe, 2, ctx->h - 16);
    lv_obj_set_pos(probe, probe_x - 1, 8);
    lv_obj_set_style_radius(probe, 0, 0);
    lv_obj_set_style_bg_color(probe, ui_token_tank_stroke(), 0);
    lv_obj_set_style_bg_opa(probe, (LV_OPA_COVER * 55) / 100, 0);
    lv_obj_set_style_border_width(probe, 0, 0);
    lv_obj_set_style_pad_all(probe, 0, 0);
    lv_obj_remove_flag(probe, LV_OBJ_FLAG_SCROLLABLE);

    /* 7. Поплавковые датчики уровня. */
    for (int i = 0; i < ctx->sw_count; i++) {
        create_level_switch(shell, ctx, i, probe_x, &cfg->switches[i]);
    }

    /* Применить начальный fill_pct (выставляет водную геометрию + текст). */
    update_water_geometry(ctx, cfg->geom.fill_pct);

    return shell;
}

void ui_tank_set_fill(lv_obj_t *tank, int pct)
{
    if (!tank) return;
    ui_tank_ctx_t *ctx = lv_obj_get_user_data(tank);
    if (!ctx) return;
    update_water_geometry(ctx, pct);
}

void ui_tank_set_switch_state(lv_obj_t *tank, int idx, ui_level_sw_state_t state)
{
    if (!tank) return;
    ui_tank_ctx_t *ctx = lv_obj_get_user_data(tank);
    if (!ctx || idx < 0 || idx >= ctx->sw_count) return;

    if (ctx->sw_dots[idx]) {
        lv_obj_set_style_bg_color(ctx->sw_dots[idx], ui_token_level_sw_fill(state), 0);
        lv_obj_set_style_border_color(ctx->sw_dots[idx], ui_token_level_sw_stroke(state), 0);
    }
    if (ctx->sw_tags[idx]) {
        lv_obj_set_style_text_color(ctx->sw_tags[idx], ui_token_level_sw_stroke(state), 0);
    }
}
