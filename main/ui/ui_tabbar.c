/**
 * @file ui_tabbar.c
 * @brief Реализация нижней tabbar (64px) — порт .tabbar из прототипа.
 */
#include "ui_tabbar.h"
#include "ui_tokens.h"
#include "ui_fonts.h"
#include "lang.h"

/** Контекст: ссылки на каждую вкладку (для подсветки). */
typedef struct {
    lv_obj_t      *tabs[UI_TAB_COUNT];     /* контейнер вкладки */
    lv_obj_t      *icons[UI_TAB_COUNT];    /* лейбл иконки */
    lv_obj_t      *labels[UI_TAB_COUNT];   /* лейбл названия */
    ui_tabbar_cb_t cb;
    ui_tab_id_t    active;
} tabbar_ctx_t;

static tabbar_ctx_t s_tb = {0};

/* Маппинг id → иконка LVGL и string-id локализации */
static const struct {
    const char *icon;        /* LV_SYMBOL_* */
    str_id_t    label_str;
} TAB_DEFS[UI_TAB_COUNT] = {
    [UI_TAB_MAIN]     = { LV_SYMBOL_HOME,     STR_NAV_MNEMONIC    },
    [UI_TAB_WASHING]  = { LV_SYMBOL_REFRESH,  STR_NAV_WASHING     },
    [UI_TAB_SETTINGS] = { LV_SYMBOL_SETTINGS, STR_NAV_SETTINGS    },
    [UI_TAB_DEBUG]    = { LV_SYMBOL_LIST,     STR_NAV_DIAGNOSTICS },
};

/* ─── оформление активной/неактивной вкладки ──────────────────────── */

static void apply_tab_style(int idx, bool active)
{
    lv_obj_t *tab   = s_tb.tabs[idx];
    lv_obj_t *icon  = s_tb.icons[idx];
    lv_obj_t *label = s_tb.labels[idx];
    if (!tab) return;

    lv_color_t fg = active ? ui_token_accent() : ui_token_text_secondary();
    lv_color_t bg = active ? ui_token_accent_mute() : ui_token_bg_card();

    lv_obj_set_style_bg_color(tab, bg, 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);

    /* Полоска сверху для активной вкладки (3px accent border-top). */
    lv_obj_set_style_border_color(tab,
        active ? ui_token_accent() : ui_token_bg_card(), 0);
    lv_obj_set_style_border_width(tab, 3, 0);
    lv_obj_set_style_border_side(tab, LV_BORDER_SIDE_TOP, 0);

    if (icon)  lv_obj_set_style_text_color(icon, fg, 0);
    if (label) lv_obj_set_style_text_color(label, fg, 0);
}

/* ─── обработчик клика ────────────────────────────────────────────── */

static void tab_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= UI_TAB_COUNT) return;
    if (s_tb.cb) s_tb.cb((ui_tab_id_t)idx);
}

/* ─── публичный API ───────────────────────────────────────────────── */

lv_obj_t *ui_tabbar_create(lv_obj_t *parent, ui_tabbar_cb_t on_select)
{
    s_tb.cb = on_select;
    s_tb.active = UI_TAB_MAIN;

    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_TABBAR_H);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, ui_token_border(), 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Каждая вкладка — UI_SCREEN_W / UI_TAB_COUNT по ширине. */
    const int tab_w = UI_SCREEN_W / UI_TAB_COUNT;

    for (int i = 0; i < UI_TAB_COUNT; i++) {
        lv_obj_t *tab = lv_obj_create(bar);
        lv_obj_set_size(tab, tab_w, UI_TABBAR_H);
        lv_obj_set_style_radius(tab, 0, 0);
        lv_obj_set_style_pad_all(tab, 0, 0);
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(tab, 2, 0);
        lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(tab, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tab, tab_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        /* Иконка (LV_SYMBOL) — одна строка */
        lv_obj_t *icon = lv_label_create(tab);
        lv_label_set_text(icon, TAB_DEFS[i].icon);
        lv_obj_set_style_text_font(icon, UI_FONT_LG, 0);

        /* Лейбл — название вкладки */
        lv_obj_t *label = lv_label_create(tab);
        lv_label_set_text(label, lang_str(TAB_DEFS[i].label_str));
        lv_obj_set_style_text_font(label, UI_FONT_SM, 0);

        s_tb.tabs[i]   = tab;
        s_tb.icons[i]  = icon;
        s_tb.labels[i] = label;

        apply_tab_style(i, i == (int)s_tb.active);
    }

    return bar;
}

void ui_tabbar_set_active(lv_obj_t *bar, ui_tab_id_t tab)
{
    (void)bar;
    if ((int)tab < 0 || tab >= UI_TAB_COUNT) return;
    if (tab == s_tb.active) return;

    apply_tab_style(s_tb.active, false);
    s_tb.active = tab;
    apply_tab_style(s_tb.active, true);
}
