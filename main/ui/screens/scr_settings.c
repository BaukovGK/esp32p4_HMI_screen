/**
 * @file scr_settings.c
 * @brief Экран "Настройки" — переработан под proto/settings.html.
 *
 * Layout (1280×692):
 *   Left nav-list (220 px): 6 категорий — Давление / Дозатор / Промывка /
 *                            Таймауты / Сеть / О системе
 *   Right panel (1fr):       Заголовок + field rows (label + desc + value)
 *                            + Actions bar (Отмена / Сохранить)
 *
 * Сейчас значения read-only (display). Numpad-редактирование будет
 * добавлено отдельным коммитом — требует MQTT-publish wiring.
 */

#include "scr_settings.h"
#include "ui_tokens.h"
#include "ui_fonts.h"
#include "ui_events.h"
#include "lang.h"
#include "plant_data.h"
#include <stdio.h>
#include <math.h>

#define PAGE_PAD       12
#define COL_GAP        12
#define NAV_W          220
#define PANEL_W        (UI_SCREEN_W - 2 * PAGE_PAD - COL_GAP - NAV_W)
#define INNER_H        (UI_CONTENT_H - 2 * PAGE_PAD)

#define CATEGORY_COUNT 6

typedef enum {
    CAT_PRESSURE = 0,
    CAT_DOSER,
    CAT_WASHING,
    CAT_TIMEOUTS,
    CAT_NETWORK,
    CAT_ABOUT,
} settings_category_t;

typedef struct {
    lv_obj_t *nav_btn[CATEGORY_COUNT];
    lv_obj_t *panel;          /* контейнер контента — пересоздаётся при смене категории */
    lv_obj_t *title_label;
    lv_obj_t *badge;
    settings_category_t current_cat;

    /* Pressure value labels (для update) */
    lv_obj_t *val_p1_max;
    lv_obj_t *val_p3_max;
    lv_obj_t *val_p4_max;
    lv_obj_t *val_filter_dp;

    /* Doser */
    lv_obj_t *val_run_time;
    lv_obj_t *val_cycle_time;

    /* Washing */
    lv_obj_t *val_wash_target_t;
    lv_obj_t *val_wash_max_t;

    /* Timeouts */
    lv_obj_t *val_pump_confirm;
    lv_obj_t *val_pump_ramp;
} settings_widgets_t;

static const struct {
    const char *name;
    const char *subtitle;
} CATEGORIES[CATEGORY_COUNT] = {
    [CAT_PRESSURE] = { "Давление",  "Уставки максимальных давлений" },
    [CAT_DOSER]    = { "Дозатор",   "Параметры реагента" },
    [CAT_WASHING]  = { "Промывка",  "Температура и тайминги CIP" },
    [CAT_TIMEOUTS] = { "Таймауты",  "Подтверждение и разгон насосов" },
    [CAT_NETWORK]  = { "Сеть",      "MQTT / Modbus / Ethernet" },
    [CAT_ABOUT]    = { "О системе", "Версия прошивки и контакты" },
};

static void widgets_free_cb(lv_event_t *e)
{
    void *p = lv_obj_get_user_data(lv_event_get_target(e));
    if (p) lv_free(p);
}

/* ─── helpers ─────────────────────────────────────────────────────── */

/** Field row: label + description (multi-line) + read-only value. */
static lv_obj_t *make_field(lv_obj_t *parent, const char *name,
                             const char *desc, const char *initial_value)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, ui_token_border(), 0);
    lv_obj_set_style_pad_ver(row, UI_GAP_MD, 0);
    lv_obj_set_style_pad_hor(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, UI_GAP_LG, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Label block (name + desc). */
    lv_obj_t *label_block = lv_obj_create(row);
    lv_obj_set_size(label_block, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(label_block, 1);
    lv_obj_set_style_bg_opa(label_block, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label_block, 0, 0);
    lv_obj_set_style_pad_all(label_block, 0, 0);
    lv_obj_set_flex_flow(label_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(label_block, 2, 0);
    lv_obj_remove_flag(label_block, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *n = lv_label_create(label_block);
    lv_label_set_text(n, name);
    lv_obj_set_style_text_color(n, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(n, UI_FONT_MD, 0);

    if (desc) {
        lv_obj_t *d = lv_label_create(label_block);
        lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(d, LV_PCT(100));
        lv_label_set_text(d, desc);
        lv_obj_set_style_text_color(d, ui_token_text_muted(), 0);
        lv_obj_set_style_text_font(d, UI_FONT_XS, 0);
    }

    /* Value (read-only). */
    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, initial_value ? initial_value : "—");
    lv_obj_set_style_text_color(v, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(v, UI_FONT_LG, 0);
    return v;
}

static void apply_panel_header(settings_widgets_t *w, settings_category_t cat)
{
    if (w->title_label) lv_label_set_text(w->title_label, CATEGORIES[cat].name);
}

/* Пересоздать содержимое панели для категории. */
static void rebuild_panel_content(settings_widgets_t *w, settings_category_t cat)
{
    if (!w->panel) return;
    /* Очистить старое содержимое (но не сам контейнер). */
    lv_obj_clean(w->panel);

    apply_panel_header(w, cat);

    /* Контейнер field rows. */
    lv_obj_t *fields = lv_obj_create(w->panel);
    lv_obj_set_size(fields, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(fields, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fields, 0, 0);
    lv_obj_set_style_pad_all(fields, 0, 0);
    lv_obj_set_flex_flow(fields, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(fields, LV_OBJ_FLAG_SCROLLABLE);

    /* Clear stale value pointers. */
    w->val_p1_max = w->val_p3_max = w->val_p4_max = w->val_filter_dp = NULL;
    w->val_run_time = w->val_cycle_time = NULL;
    w->val_wash_target_t = w->val_wash_max_t = NULL;
    w->val_pump_confirm = w->val_pump_ramp = NULL;

    switch (cat) {
    case CAT_PRESSURE:
        w->val_p1_max     = make_field(fields, "Макс. P1 (преднагн.)",
            "Превышение → ALARM, останов преднагнетания  ·  диапазон 0..6 bar", "5.5 bar");
        w->val_p3_max     = make_field(fields, "Макс. P3 (1-я ст.)",
            "Превышение → ALARM, останов насоса 1-й ст.  ·  диапазон 0..40 bar", "35.0 bar");
        w->val_p4_max     = make_field(fields, "Макс. P4 (2-я ст.)",
            "Превышение → ALARM, останов насоса 2-й ст.  ·  диапазон 0..10 bar", "8.0 bar");
        w->val_filter_dp  = make_field(fields, "ΔP фильтра (warn)",
            "Перепад P1 − P2 → WARNING, засорение фильтра  ·  0.5..3.0 bar", "1.0 bar");
        break;

    case CAT_DOSER:
        w->val_run_time   = make_field(fields, "Время работы дозатора",
            "Длительность включения насоса-дозатора реагента", "5 мин");
        w->val_cycle_time = make_field(fields, "Время цикла",
            "Период между включениями дозатора", "60 мин");
        break;

    case CAT_WASHING:
        w->val_wash_target_t = make_field(fields, "Целевая температура",
            "Температура раствора при которой система переходит к фазе SUPPLY", "35.0 °C");
        w->val_wash_max_t    = make_field(fields, "Макс. температура",
            "Аварийная уставка — превышение немедленно останавливает нагреватель", "40.0 °C");
        make_field(fields, "Время нагрева (макс.)",
            "Таймаут на достижение целевой температуры. Превышение → ALARM", "30 мин");
        make_field(fields, "Время подачи раствора",
            "Длительность фазы циркуляции с открытыми клапанами", "20 мин");
        make_field(fields, "Время слива",
            "Длительность фазы дренажа отработанного раствора", "5 мин");
        break;

    case CAT_TIMEOUTS:
        w->val_pump_confirm = make_field(fields, "Подтверждение насоса",
            "Время ожидания DI confirmation после команды старта. Если DI не сработал → FAULT", "3000 мс");
        w->val_pump_ramp    = make_field(fields, "Время разгона",
            "Время выхода насоса на номинальные обороты — игнорируем превышения уставок в этот период", "15000 мс");
        break;

    case CAT_NETWORK:
        make_field(fields, "MQTT broker",
            "Адрес и порт MQTT-брокера для связи с контроллером", "192.168.1.10:1883");
        make_field(fields, "Modbus baudrate",
            "Скорость RS-485 шины Modbus (8N1, half-duplex)", "9600");
        make_field(fields, "Ethernet",
            "Статический IP или DHCP", "192.168.1.20 (DHCP)");
        break;

    case CAT_ABOUT:
        make_field(fields, "Прошивка HMI",
            "Версия LVGL-интерфейса на ESP32-P4-NANO", "v1.0.0-dev");
        make_field(fields, "Контроллер",
            "Версия прошивки RO Controller ESP32-S3", "v1.2.3");
        make_field(fields, "LVGL",
            "Графическая библиотека интерфейса", "9.2.2");
        make_field(fields, "Контакты",
            "Поддержка: support@example.com", "");
        break;
    }

    /* Actions bar — только для редактируемых категорий. */
    if (cat == CAT_PRESSURE || cat == CAT_DOSER ||
        cat == CAT_WASHING  || cat == CAT_TIMEOUTS) {
        lv_obj_t *actions = lv_obj_create(w->panel);
        lv_obj_set_size(actions, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(actions, 0, 0);
        lv_obj_set_style_pad_top(actions, UI_GAP_LG, 0);
        lv_obj_set_style_pad_bottom(actions, 0, 0);
        lv_obj_set_style_pad_hor(actions, 0, 0);
        lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(actions, UI_GAP_SM, 0);
        lv_obj_remove_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *btn_cancel = lv_button_create(actions);
        lv_obj_set_size(btn_cancel, 120, 40);
        lv_obj_set_style_bg_color(btn_cancel, ui_token_bg_mute(), 0);
        lv_obj_set_style_border_color(btn_cancel, ui_token_border(), 0);
        lv_obj_set_style_border_width(btn_cancel, 1, 0);
        lv_obj_set_style_radius(btn_cancel, UI_RADIUS_MD, 0);
        lv_obj_set_style_shadow_width(btn_cancel, 0, 0);
        lv_obj_t *cl = lv_label_create(btn_cancel);
        lv_label_set_text(cl, "Отмена");
        lv_obj_set_style_text_color(cl, ui_token_text_primary(), 0);
        lv_obj_set_style_text_font(cl, UI_FONT_MD, 0);
        lv_obj_center(cl);

        lv_obj_t *btn_save = lv_button_create(actions);
        lv_obj_set_size(btn_save, 140, 40);
        lv_obj_set_style_bg_color(btn_save, ui_token_accent(), 0);
        lv_obj_set_style_radius(btn_save, UI_RADIUS_MD, 0);
        lv_obj_set_style_shadow_width(btn_save, 0, 0);
        lv_obj_set_style_border_width(btn_save, 0, 0);
        lv_obj_t *sl = lv_label_create(btn_save);
        lv_label_set_text(sl, "Сохранить");
        lv_obj_set_style_text_color(sl, ui_token_text_inverse(), 0);
        lv_obj_set_style_text_font(sl, UI_FONT_MD, 0);
        lv_obj_center(sl);
        /* TODO: при клике публиковать settings_*_t через mqtt_publish_*. */
    }
}

static void apply_nav_style(settings_widgets_t *w, int idx)
{
    for (int i = 0; i < CATEGORY_COUNT; i++) {
        if (!w->nav_btn[i]) continue;
        bool active = (i == idx);
        lv_obj_set_style_bg_color(w->nav_btn[i],
            active ? ui_token_accent_mute() : ui_token_bg_card(), 0);
        lv_obj_t *lbl = lv_obj_get_child(w->nav_btn[i], 0);
        if (lbl) {
            lv_obj_set_style_text_color(lbl,
                active ? ui_token_accent() : ui_token_text_primary(), 0);
        }
    }
}

static void nav_btn_event_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *root = lv_obj_get_screen(btn);  /* fallback */
    /* Найти root scr_settings: ищем родителя с user_data=settings_widgets_t */
    lv_obj_t *p = lv_obj_get_parent(btn);
    while (p && lv_obj_get_user_data(p) == NULL) {
        p = lv_obj_get_parent(p);
    }
    if (!p) return;
    settings_widgets_t *w = lv_obj_get_user_data(p);
    if (!w) return;

    w->current_cat = (settings_category_t)idx;
    apply_nav_style(w, idx);
    rebuild_panel_content(w, w->current_cat);
}

/* ─── create screen ──────────────────────────────────────────────── */

lv_obj_t *scr_settings_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, ui_token_bg_base(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    settings_widgets_t *w = lv_malloc_zeroed(sizeof(*w));
    if (!w) return root;
    lv_obj_set_user_data(root, w);
    lv_obj_add_event_cb(root, widgets_free_cb, LV_EVENT_DELETE, NULL);

    /* ─── Left: nav-list ─────────────────────────────────────────── */
    lv_obj_t *nav = lv_obj_create(root);
    lv_obj_set_pos(nav, PAGE_PAD, PAGE_PAD);
    lv_obj_set_size(nav, NAV_W, INNER_H);
    lv_obj_set_style_bg_color(nav, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(nav, ui_token_border(), 0);
    lv_obj_set_style_border_width(nav, 1, 0);
    lv_obj_set_style_radius(nav, UI_RADIUS_LG, 0);
    lv_obj_set_style_pad_all(nav, UI_GAP_SM, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(nav, 4, 0);
    lv_obj_remove_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < CATEGORY_COUNT; i++) {
        lv_obj_t *btn = lv_obj_create(nav);
        lv_obj_set_size(btn, LV_PCT(100), 48);
        lv_obj_set_style_bg_color(btn, ui_token_bg_card(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, UI_RADIUS_MD, 0);
        lv_obj_set_style_pad_hor(btn, UI_GAP_MD, 0);
        lv_obj_set_style_pad_ver(btn, 0, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, nav_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        w->nav_btn[i] = btn;

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, CATEGORIES[i].name);
        lv_obj_set_style_text_color(lbl, ui_token_text_primary(), 0);
        lv_obj_set_style_text_font(lbl, UI_FONT_MD, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    }

    /* ─── Right: content panel ────────────────────────────────────── */
    int panel_x = PAGE_PAD + NAV_W + COL_GAP;

    /* Card wrapper — заголовок + контент. */
    lv_obj_t *card = lv_obj_create(root);
    lv_obj_set_pos(card, panel_x, PAGE_PAD);
    lv_obj_set_size(card, PANEL_W, INNER_H);
    lv_obj_set_style_bg_color(card, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, ui_token_border(), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, UI_RADIUS_LG, 0);
    lv_obj_set_style_pad_all(card, UI_GAP_LG, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, UI_GAP_MD, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Header row: title (large). */
    w->title_label = lv_label_create(card);
    lv_label_set_text(w->title_label, CATEGORIES[CAT_PRESSURE].name);
    lv_obj_set_style_text_color(w->title_label, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(w->title_label, UI_FONT_LG, 0);

    /* Content panel — scrollable. */
    w->panel = lv_obj_create(card);
    lv_obj_set_size(w->panel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(w->panel, 1);
    lv_obj_set_style_bg_opa(w->panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(w->panel, 0, 0);
    lv_obj_set_style_pad_all(w->panel, 0, 0);
    lv_obj_set_flex_flow(w->panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(w->panel, LV_DIR_VER);

    /* Init: показать pressure по умолчанию. */
    w->current_cat = CAT_PRESSURE;
    apply_nav_style(w, CAT_PRESSURE);
    rebuild_panel_content(w, CAT_PRESSURE);

    return root;
}

/* ─── update ─────────────────────────────────────────────────────── */

void scr_settings_update(lv_obj_t *container, const plant_data_t *data, uint32_t dirty)
{
    (void)dirty;
    if (!container || !data) return;
    settings_widgets_t *w = lv_obj_get_user_data(container);
    if (!w) return;

    char buf[24];

    /* Pressure category. */
    if (w->val_p1_max) {
        snprintf(buf, sizeof(buf), "%.1f bar", data->set_pressure.p1_max);
        lv_label_set_text(w->val_p1_max, buf);
    }
    if (w->val_p3_max) {
        snprintf(buf, sizeof(buf), "%.1f bar", data->set_pressure.p3_max);
        lv_label_set_text(w->val_p3_max, buf);
    }
    if (w->val_p4_max) {
        snprintf(buf, sizeof(buf), "%.1f bar", data->set_pressure.p4_max);
        lv_label_set_text(w->val_p4_max, buf);
    }
    if (w->val_filter_dp) {
        snprintf(buf, sizeof(buf), "%.1f bar", data->set_pressure.filter_dp_warn);
        lv_label_set_text(w->val_filter_dp, buf);
    }

    /* Doser. */
    if (w->val_run_time) {
        snprintf(buf, sizeof(buf), "%d мин", data->set_doser.run_time_min);
        lv_label_set_text(w->val_run_time, buf);
    }
    if (w->val_cycle_time) {
        snprintf(buf, sizeof(buf), "%d мин", data->set_doser.cycle_time_min);
        lv_label_set_text(w->val_cycle_time, buf);
    }

    /* Washing. */
    if (w->val_wash_target_t) {
        snprintf(buf, sizeof(buf), "%.1f °C", data->set_washing.target_temp_C);
        lv_label_set_text(w->val_wash_target_t, buf);
    }
    if (w->val_wash_max_t) {
        snprintf(buf, sizeof(buf), "%.1f °C", data->set_washing.max_temp_C);
        lv_label_set_text(w->val_wash_max_t, buf);
    }

    /* Timeouts. */
    if (w->val_pump_confirm) {
        snprintf(buf, sizeof(buf), "%d мс", data->set_timeouts.pump_confirm_ms);
        lv_label_set_text(w->val_pump_confirm, buf);
    }
    if (w->val_pump_ramp) {
        snprintf(buf, sizeof(buf), "%d мс", data->set_timeouts.pump_ramp_ms);
        lv_label_set_text(w->val_pump_ramp, buf);
    }
}
