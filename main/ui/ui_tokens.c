/**
 * @file ui_tokens.c
 * @brief Реализация дизайн-токенов: статические таблицы цветов под light/dark.
 *
 * Источник истины — proto/style.css. Любое изменение CSS-переменной должно
 * быть синхронно отражено здесь. См. docs/UI_TOKENS.md.
 */
#include "ui_tokens.h"

/* HEX(0xRRGGBB) → (lv_color_t){blue, green, red} — валидный compound literal
 * под layout LVGL 9 (`{ uint8_t blue; uint8_t green; uint8_t red; }`).
 * Можно использовать в static-initializer структур. */
#define HEX(h)  ((lv_color_t)LV_COLOR_MAKE(((h) >> 16) & 0xff, \
                                            ((h) >> 8)  & 0xff, \
                                             (h)        & 0xff))

/* Структура палитры. Каждое поле — токен из proto/style.css. */
typedef struct {
    /* поверхности */
    lv_color_t bg_base;
    lv_color_t bg_card;
    lv_color_t bg_mute;
    lv_color_t bg_elev;
    lv_color_t border;
    lv_color_t border_strong;
    /* текст */
    lv_color_t text_primary;
    lv_color_t text_secondary;
    lv_color_t text_muted;
    lv_color_t text_inverse;
    /* бренд */
    lv_color_t accent;
    lv_color_t accent_hover;
    lv_color_t accent_mute;
    /* состояния */
    lv_color_t success;
    lv_color_t warning;
    lv_color_t danger;
    lv_color_t info;
    /* технологические */
    lv_color_t pipe;
    lv_color_t pipe_active;
    lv_color_t tank_stroke;
    lv_color_t water;
} ui_palette_t;

/* ─── light theme — proto/style.css :root ─────────────────────────── */
static const ui_palette_t s_light = {
    /* поверхности */
    .bg_base       = HEX(0xf5f7fa),
    .bg_card       = HEX(0xffffff),
    .bg_mute       = HEX(0xeef0f3),
    .bg_elev       = HEX(0xe1e5eb),
    .border        = HEX(0xd5d9e0),
    .border_strong = HEX(0xb0b6c0),
    /* текст */
    .text_primary   = HEX(0x1a1d23),
    .text_secondary = HEX(0x5a6068),
    .text_muted     = HEX(0x8a9099),
    .text_inverse   = HEX(0xffffff),
    /* бренд НТТ */
    .accent       = HEX(0x2d8659),
    .accent_hover = HEX(0x1f6b45),
    .accent_mute  = HEX(0xdcefe4),
    /* состояния */
    .success = HEX(0x16a34a),
    .warning = HEX(0xf59e0b),
    .danger  = HEX(0xdc2626),
    .info    = HEX(0x0284c7),
    /* технологические */
    .pipe        = HEX(0x9aa0a6),
    .pipe_active = HEX(0x2d8659),    /* = accent */
    .tank_stroke = HEX(0x475569),
    .water       = HEX(0x38bdf8),
};

/* ─── dark theme — proto/style.css [data-theme="dark"] ────────────── */
static const ui_palette_t s_dark = {
    /* поверхности */
    .bg_base       = HEX(0x14171d),
    .bg_card       = HEX(0x252932),
    .bg_mute       = HEX(0x1a1d23),
    .bg_elev       = HEX(0x2f3540),
    .border        = HEX(0x383e48),
    .border_strong = HEX(0x4a5060),
    /* текст */
    .text_primary   = HEX(0xe6e9ed),
    .text_secondary = HEX(0xa8aeb6),
    .text_muted     = HEX(0x6c727b),
    .text_inverse   = HEX(0x1e2128),
    /* бренд */
    .accent       = HEX(0x3fa66a),
    .accent_hover = HEX(0x52c179),
    .accent_mute  = HEX(0x1a3826),
    /* состояния */
    .success = HEX(0x22c55e),
    .warning = HEX(0xfbbf24),
    .danger  = HEX(0xef4444),
    .info    = HEX(0x38bdf8),
    /* технологические */
    .pipe        = HEX(0x6c727b),
    .pipe_active = HEX(0x3fa66a),
    .tank_stroke = HEX(0x94a3b8),
    .water       = HEX(0x38bdf8),
};

/* ─── активная палитра ─────────────────────────────────────────────── */
static ui_theme_id_t s_theme = UI_THEME_LIGHT;
static const ui_palette_t *s_active = &s_light;

void ui_tokens_set_theme(ui_theme_id_t theme)
{
    s_theme  = theme;
    s_active = (theme == UI_THEME_DARK) ? &s_dark : &s_light;
}

ui_theme_id_t ui_tokens_get_theme(void)
{
    return s_theme;
}

/* ─── accessors ────────────────────────────────────────────────────── */

lv_color_t ui_token_bg_base(void)        { return s_active->bg_base; }
lv_color_t ui_token_bg_card(void)        { return s_active->bg_card; }
lv_color_t ui_token_bg_mute(void)        { return s_active->bg_mute; }
lv_color_t ui_token_bg_elev(void)        { return s_active->bg_elev; }
lv_color_t ui_token_border(void)         { return s_active->border; }
lv_color_t ui_token_border_strong(void)  { return s_active->border_strong; }

lv_color_t ui_token_text_primary(void)   { return s_active->text_primary; }
lv_color_t ui_token_text_secondary(void) { return s_active->text_secondary; }
lv_color_t ui_token_text_muted(void)     { return s_active->text_muted; }
lv_color_t ui_token_text_inverse(void)   { return s_active->text_inverse; }

lv_color_t ui_token_accent(void)         { return s_active->accent; }
lv_color_t ui_token_accent_hover(void)   { return s_active->accent_hover; }
lv_color_t ui_token_accent_mute(void)    { return s_active->accent_mute; }

lv_color_t ui_token_success(void)        { return s_active->success; }
lv_color_t ui_token_warning(void)        { return s_active->warning; }
lv_color_t ui_token_danger(void)         { return s_active->danger; }
lv_color_t ui_token_info(void)           { return s_active->info; }

lv_color_t ui_token_pipe(void)           { return s_active->pipe; }
lv_color_t ui_token_pipe_active(void)    { return s_active->pipe_active; }
lv_color_t ui_token_tank_stroke(void)    { return s_active->tank_stroke; }
lv_color_t ui_token_water(void)          { return s_active->water; }

/* ─── pump-circle: state → color ───────────────────────────────────── */

lv_color_t ui_token_pump_bg(ui_pump_state_t state)
{
    switch (state) {
    case UI_PUMP_RUNNING:  return s_active->accent;        /* gradient — fallback solid */
    case UI_PUMP_STARTING: return s_active->warning;
    case UI_PUMP_ERROR:    return s_active->danger;
    case UI_PUMP_OFF:
    default:               return s_active->bg_mute;
    }
}

lv_color_t ui_token_pump_stroke(ui_pump_state_t state)
{
    switch (state) {
    case UI_PUMP_RUNNING:  return s_active->accent;
    case UI_PUMP_STARTING: return s_active->warning;
    case UI_PUMP_ERROR:    return s_active->danger;
    case UI_PUMP_OFF:
    default:               return s_active->border_strong;
    }
}

lv_color_t ui_token_pump_blade(ui_pump_state_t state)
{
    switch (state) {
    case UI_PUMP_RUNNING:  return HEX(0xf4faf6);   /* почти белые лепестки */
    case UI_PUMP_STARTING: return s_active->warning;
    case UI_PUMP_ERROR:    return s_active->danger;
    case UI_PUMP_OFF:
    default:               return s_active->text_muted;
    }
}

/* ─── level-sw: state → color ──────────────────────────────────────── */

lv_color_t ui_token_level_sw_fill(ui_level_sw_state_t state)
{
    switch (state) {
    case UI_LEVEL_SW_ACTIVE: return s_active->accent;
    case UI_LEVEL_SW_ALARM:  return s_active->danger;
    case UI_LEVEL_SW_DRY:
    default:                 return s_active->bg_card;
    }
}

lv_color_t ui_token_level_sw_stroke(ui_level_sw_state_t state)
{
    switch (state) {
    case UI_LEVEL_SW_ACTIVE: return s_active->accent;
    case UI_LEVEL_SW_ALARM:  return s_active->danger;
    case UI_LEVEL_SW_DRY:
    default:                 return s_active->text_muted;
    }
}
