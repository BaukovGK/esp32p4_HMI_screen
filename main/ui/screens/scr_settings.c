/*
 * scr_settings.c -- Settings screen with tabview + numeric keypad
 *
 * Left side  (~800px): 4 tabs (Pressure, Doser, Washing, Timeouts)
 * Right side (~460px): Numeric keypad for touchscreen input
 *
 * Content area: 1280 x 700 px.
 */

#include "scr_settings.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_events.h"
#include "ui_fonts.h"
#include "lang.h"
#include "mqtt_app.h"
#include "plant_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- Widget storage ---- */

typedef struct {
    /* Pressure tab */
    lv_obj_t *ta_p1_max;
    lv_obj_t *ta_p3_max;
    lv_obj_t *ta_p4_max;
    lv_obj_t *ta_filter_dp_warn;

    /* Doser tab */
    lv_obj_t *ta_run_time;
    lv_obj_t *ta_cycle_time;

    /* Washing tab */
    lv_obj_t *ta_target_temp;
    lv_obj_t *ta_max_temp;
    lv_obj_t *ta_overshoot;
    lv_obj_t *ta_hysteresis;
    lv_obj_t *ta_heat_timeout;
    lv_obj_t *ta_supply_time;
    lv_obj_t *ta_drain_time;

    /* Timeouts tab */
    lv_obj_t *ta_pump_confirm;
    lv_obj_t *ta_pump_ramp;

    /* Currently focused textarea (for numpad) */
    lv_obj_t *active_ta;
} settings_widgets_t;

/* Keep a module-level pointer for event callbacks */
static settings_widgets_t *s_widgets = NULL;

/* ---- Helpers ---- */

static void ta_select_cb(lv_event_t *e)
{
    if (!s_widgets) return;

    /* Remove highlight from previous textarea */
    if (s_widgets->active_ta) {
        lv_obj_set_style_border_color(s_widgets->active_ta, COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_widgets->active_ta, 1, 0);
    }

    /* Set new active textarea and highlight it */
    s_widgets->active_ta = lv_event_get_target(e);
    lv_obj_set_style_border_color(s_widgets->active_ta, lv_color_hex(0x00FF88), 0);
    lv_obj_set_style_border_width(s_widgets->active_ta, 2, 0);
}

/* Create a labeled textarea row inside a parent column */
static lv_obj_t *make_field(lv_obj_t *parent, const char *label_text,
                            const char *initial_value)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 16, 0);
    lv_obj_set_style_pad_ver(row, 4, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_width(lbl, 220);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_16, 0);

    lv_obj_t *ta = lv_textarea_create(row);
    lv_obj_set_size(ta, 160, 44);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, initial_value);
    lv_obj_set_style_bg_color(ta, COLOR_BG_WIDGET, 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ta, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(ta, UI_FONT_16, 0);
    lv_obj_set_style_border_color(ta, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_radius(ta, 4, 0);

    /* Track selection for numpad (CLICKED works reliably with touch) */
    lv_obj_add_event_cb(ta, ta_select_cb, LV_EVENT_CLICKED, NULL);

    return ta;
}

static float read_ta_float(lv_obj_t *ta)
{
    const char *txt = lv_textarea_get_text(ta);
    if (!txt || txt[0] == '\0') return 0.0f;
    return strtof(txt, NULL);
}

static int read_ta_int(lv_obj_t *ta)
{
    const char *txt = lv_textarea_get_text(ta);
    if (!txt || txt[0] == '\0') return 0;
    return (int)strtol(txt, NULL, 10);
}

/* ---- Event: Apply Pressure ---- */
static void evt_apply_pressure(lv_event_t *e)
{
    settings_widgets_t *w = (settings_widgets_t *)lv_event_get_user_data(e);
    settings_pressure_t s = {
        .p1_max         = read_ta_float(w->ta_p1_max),
        .p3_max         = read_ta_float(w->ta_p3_max),
        .p4_max         = read_ta_float(w->ta_p4_max),
        .filter_dp_warn = read_ta_float(w->ta_filter_dp_warn),
    };
    mqtt_publish_settings_pressure(&s);
}

/* ---- Event: Apply Doser ---- */
static void evt_apply_doser(lv_event_t *e)
{
    settings_widgets_t *w = (settings_widgets_t *)lv_event_get_user_data(e);
    settings_doser_t s = {
        .run_time_min   = read_ta_int(w->ta_run_time),
        .cycle_time_min = read_ta_int(w->ta_cycle_time),
    };
    mqtt_publish_settings_doser(&s);
}

/* ---- Event: Apply Washing ---- */
static void evt_apply_washing(lv_event_t *e)
{
    settings_widgets_t *w = (settings_widgets_t *)lv_event_get_user_data(e);
    settings_washing_t s = {
        .target_temp_C   = read_ta_float(w->ta_target_temp),
        .max_temp_C      = read_ta_float(w->ta_max_temp),
        .t_overshoot_C   = read_ta_float(w->ta_overshoot),
        .hysteresis_C    = read_ta_float(w->ta_hysteresis),
        .heat_timeout_min = read_ta_int(w->ta_heat_timeout),
        .supply_time_min = read_ta_int(w->ta_supply_time),
        .drain_time_min  = read_ta_int(w->ta_drain_time),
    };
    mqtt_publish_settings_washing(&s);
}

/* ---- Event: Apply Timeouts ---- */
static void evt_apply_timeouts(lv_event_t *e)
{
    settings_widgets_t *w = (settings_widgets_t *)lv_event_get_user_data(e);
    settings_timeouts_t s = {
        .pump_confirm_ms = read_ta_int(w->ta_pump_confirm),
        .pump_ramp_ms    = read_ta_int(w->ta_pump_ramp),
    };
    mqtt_publish_settings_timeouts(&s);
}

/* Make an APPLY button at the bottom of a tab */
static lv_obj_t *make_apply_btn(lv_obj_t *tab, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(tab);
    lv_obj_set_size(btn, 200, 50);
    lv_obj_set_style_bg_color(btn, COLOR_BTN_PRIMARY, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_margin_top(btn, 16, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, lang_str(STR_BTN_APPLY));
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_18, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}

/* ---- Numpad callbacks ---- */

static void numpad_digit_cb(lv_event_t *e)
{
    if (!s_widgets || !s_widgets->active_ta) return;
    const char *ch = (const char *)lv_event_get_user_data(e);
    lv_textarea_add_text(s_widgets->active_ta, ch);
}

static void numpad_del_cb(lv_event_t *e)
{
    (void)e;
    if (!s_widgets || !s_widgets->active_ta) return;
    lv_textarea_delete_char(s_widgets->active_ta);
}

static void numpad_ac_cb(lv_event_t *e)
{
    (void)e;
    if (!s_widgets || !s_widgets->active_ta) return;
    lv_textarea_set_text(s_widgets->active_ta, "");
}

static void numpad_enter_cb(lv_event_t *e)
{
    (void)e;
    if (!s_widgets || !s_widgets->active_ta) return;
    /* Defocus the textarea */
    lv_obj_remove_state(s_widgets->active_ta, LV_STATE_FOCUSED);
    lv_obj_remove_state(s_widgets->active_ta, LV_STATE_FOCUS_KEY);
}

/* ---- Numpad builder ---- */

static lv_obj_t *make_numpad_btn(lv_obj_t *parent, const char *text,
                                  int32_t w, int32_t h,
                                  lv_color_t bg, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_22, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);
    return btn;
}

/* Static digit strings for user_data pointers (must outlive callbacks) */
static const char *s_digits[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "."
};

static lv_obj_t *create_numpad(lv_obj_t *parent, int32_t x, int32_t y)
{
    lv_obj_t *pad = lv_obj_create(parent);
    lv_obj_remove_style_all(pad);
    lv_obj_set_size(pad, 420, 390);
    lv_obj_set_pos(pad, x, y);
    lv_obj_set_style_bg_color(pad, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pad, 12, 0);
    lv_obj_set_style_pad_all(pad, 16, 0);
    lv_obj_remove_flag(pad, LV_OBJ_FLAG_SCROLLABLE);

    /*
     * Numpad layout (4 columns x 5 rows):
     *
     *  [7]  [8]  [9]  [Del]
     *  [4]  [5]  [6]  [AC ]
     *  [1]  [2]  [3]  [ . ]
     *  [    0    ] [  OK   ]
     */

    const int32_t bw = 88;     /* button width */
    const int32_t bh = 68;     /* button height */
    const int32_t gap = 8;     /* gap between buttons */
    lv_color_t bg_digit = COLOR_BG_WIDGET;
    lv_color_t bg_func  = lv_color_hex(0x2A3E5C);
    lv_color_t bg_enter = COLOR_BTN_PRIMARY;

    /* Row 0: 7 8 9 Del */
    int32_t ry = 0;
    make_numpad_btn(pad, "7", bw, bh, bg_digit, numpad_digit_cb, (void *)s_digits[7]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 0, ry);
    make_numpad_btn(pad, "8", bw, bh, bg_digit, numpad_digit_cb, (void *)s_digits[8]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), bw + gap, ry);
    make_numpad_btn(pad, "9", bw, bh, bg_digit, numpad_digit_cb, (void *)s_digits[9]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 2 * (bw + gap), ry);
    make_numpad_btn(pad, "Del", bw, bh, bg_func, numpad_del_cb, NULL);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 3 * (bw + gap), ry);

    /* Row 1: 4 5 6 AC */
    ry = bh + gap;
    make_numpad_btn(pad, "4", bw, bh, bg_digit, numpad_digit_cb, (void *)s_digits[4]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 0, ry);
    make_numpad_btn(pad, "5", bw, bh, bg_digit, numpad_digit_cb, (void *)s_digits[5]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), bw + gap, ry);
    make_numpad_btn(pad, "6", bw, bh, bg_digit, numpad_digit_cb, (void *)s_digits[6]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 2 * (bw + gap), ry);
    make_numpad_btn(pad, "AC", bw, bh, COLOR_BTN_DANGER, numpad_ac_cb, NULL);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 3 * (bw + gap), ry);

    /* Row 2: 1 2 3 . */
    ry = 2 * (bh + gap);
    make_numpad_btn(pad, "1", bw, bh, bg_digit, numpad_digit_cb, (void *)s_digits[1]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 0, ry);
    make_numpad_btn(pad, "2", bw, bh, bg_digit, numpad_digit_cb, (void *)s_digits[2]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), bw + gap, ry);
    make_numpad_btn(pad, "3", bw, bh, bg_digit, numpad_digit_cb, (void *)s_digits[3]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 2 * (bw + gap), ry);
    make_numpad_btn(pad, ".", bw, bh, bg_func, numpad_digit_cb, (void *)s_digits[10]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 3 * (bw + gap), ry);

    /* Row 3: [  0  ]  [ OK ] */
    ry = 3 * (bh + gap);
    int32_t wide = 2 * bw + gap;    /* double-wide button */
    make_numpad_btn(pad, "0", wide, bh, bg_digit, numpad_digit_cb, (void *)s_digits[0]);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), 0, ry);
    make_numpad_btn(pad, "OK", wide, bh, bg_enter, numpad_enter_cb, NULL);
    lv_obj_set_pos(lv_obj_get_child(pad, -1), wide + gap, ry);

    return pad;
}

/* ---- Create ---- */

lv_obj_t *scr_settings_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 1280, 700);
    lv_obj_set_style_bg_color(cont, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    settings_widgets_t *w = lv_malloc(sizeof(settings_widgets_t));
    lv_memzero(w, sizeof(settings_widgets_t));
    s_widgets = w;

    /* ---- Tabview (left 800px) ---- */
    lv_obj_t *tv = lv_tabview_create(cont);
    lv_obj_set_size(tv, 800, 700);
    lv_obj_set_pos(tv, 0, 0);
    lv_obj_set_style_bg_color(tv, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(tv, LV_OPA_COVER, 0);

    /* Style tab buttons */
    lv_obj_t *tab_btns = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(tab_btns, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(tab_btns, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(tab_btns, UI_FONT_16, 0);

    /* ---- Tab 1: Pressure ---- */
    lv_obj_t *t1 = lv_tabview_add_tab(tv, lang_str(STR_SET_PRESSURE));
    lv_obj_set_layout(t1, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(t1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(t1, 16, 0);
    lv_obj_set_style_pad_row(t1, 4, 0);

    w->ta_p1_max         = make_field(t1, "P1 max (bar)",       "");
    w->ta_p3_max         = make_field(t1, "P3 max (bar)",       "");
    w->ta_p4_max         = make_field(t1, "P4 max (bar)",       "");
    w->ta_filter_dp_warn = make_field(t1, "Filter dP warn (bar)", "");
    make_apply_btn(t1, evt_apply_pressure, w);

    /* ---- Tab 2: Doser ---- */
    lv_obj_t *t2 = lv_tabview_add_tab(tv, lang_str(STR_SET_DOSER));
    lv_obj_set_layout(t2, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(t2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(t2, 16, 0);
    lv_obj_set_style_pad_row(t2, 4, 0);

    w->ta_run_time   = make_field(t2, "Run time (min)",   "");
    w->ta_cycle_time = make_field(t2, "Cycle time (min)", "");
    make_apply_btn(t2, evt_apply_doser, w);

    /* ---- Tab 3: Washing ---- */
    lv_obj_t *t3 = lv_tabview_add_tab(tv, lang_str(STR_SET_WASHING));
    lv_obj_set_layout(t3, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(t3, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(t3, 16, 0);
    lv_obj_set_style_pad_row(t3, 4, 0);

    w->ta_target_temp  = make_field(t3, "Target temp (\xc2\xb0\x43)",    "");
    w->ta_max_temp     = make_field(t3, "Max temp (\xc2\xb0\x43)",       "");
    w->ta_overshoot    = make_field(t3, "Overshoot (\xc2\xb0\x43)",      "");
    w->ta_hysteresis   = make_field(t3, "Hysteresis (\xc2\xb0\x43)",     "");
    w->ta_heat_timeout = make_field(t3, "Heat timeout (min)",  "");
    w->ta_supply_time  = make_field(t3, "Supply time (min)",   "");
    w->ta_drain_time   = make_field(t3, "Drain time (min)",    "");
    make_apply_btn(t3, evt_apply_washing, w);

    /* ---- Tab 4: Timeouts ---- */
    lv_obj_t *t4 = lv_tabview_add_tab(tv, lang_str(STR_SET_TIMEOUTS));
    lv_obj_set_layout(t4, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(t4, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(t4, 16, 0);
    lv_obj_set_style_pad_row(t4, 4, 0);

    w->ta_pump_confirm = make_field(t4, "Pump confirm (ms)", "");
    w->ta_pump_ramp    = make_field(t4, "Pump ramp (ms)",    "");
    make_apply_btn(t4, evt_apply_timeouts, w);

    /* ---- Numeric Keypad (right side) ---- */
    create_numpad(cont, 830, 150);

    /* No textarea selected until user taps one */
    w->active_ta = NULL;

    lv_obj_set_user_data(cont, w);
    return cont;
}

/* ---- Update ---- */

void scr_settings_update(lv_obj_t *container, const plant_data_t *d)
{
    settings_widgets_t *w = (settings_widgets_t *)lv_obj_get_user_data(container);
    if (!w) return;

    /*
     * Only load values into text areas if they are currently empty.
     * This prevents overwriting user edits while still providing
     * initial values from the plant_data settings cache.
     */
    char buf[32];

    /* Pressure */
    if (strlen(lv_textarea_get_text(w->ta_p1_max)) == 0) {
        snprintf(buf, sizeof(buf), "%.2f", (double)d->set_pressure.p1_max);
        lv_textarea_set_text(w->ta_p1_max, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_p3_max)) == 0) {
        snprintf(buf, sizeof(buf), "%.2f", (double)d->set_pressure.p3_max);
        lv_textarea_set_text(w->ta_p3_max, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_p4_max)) == 0) {
        snprintf(buf, sizeof(buf), "%.2f", (double)d->set_pressure.p4_max);
        lv_textarea_set_text(w->ta_p4_max, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_filter_dp_warn)) == 0) {
        snprintf(buf, sizeof(buf), "%.2f", (double)d->set_pressure.filter_dp_warn);
        lv_textarea_set_text(w->ta_filter_dp_warn, buf);
    }

    /* Doser */
    if (strlen(lv_textarea_get_text(w->ta_run_time)) == 0) {
        snprintf(buf, sizeof(buf), "%d", d->set_doser.run_time_min);
        lv_textarea_set_text(w->ta_run_time, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_cycle_time)) == 0) {
        snprintf(buf, sizeof(buf), "%d", d->set_doser.cycle_time_min);
        lv_textarea_set_text(w->ta_cycle_time, buf);
    }

    /* Washing */
    if (strlen(lv_textarea_get_text(w->ta_target_temp)) == 0) {
        snprintf(buf, sizeof(buf), "%.1f", (double)d->set_washing.target_temp_C);
        lv_textarea_set_text(w->ta_target_temp, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_max_temp)) == 0) {
        snprintf(buf, sizeof(buf), "%.1f", (double)d->set_washing.max_temp_C);
        lv_textarea_set_text(w->ta_max_temp, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_overshoot)) == 0) {
        snprintf(buf, sizeof(buf), "%.1f", (double)d->set_washing.t_overshoot_C);
        lv_textarea_set_text(w->ta_overshoot, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_hysteresis)) == 0) {
        snprintf(buf, sizeof(buf), "%.1f", (double)d->set_washing.hysteresis_C);
        lv_textarea_set_text(w->ta_hysteresis, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_heat_timeout)) == 0) {
        snprintf(buf, sizeof(buf), "%d", d->set_washing.heat_timeout_min);
        lv_textarea_set_text(w->ta_heat_timeout, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_supply_time)) == 0) {
        snprintf(buf, sizeof(buf), "%d", d->set_washing.supply_time_min);
        lv_textarea_set_text(w->ta_supply_time, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_drain_time)) == 0) {
        snprintf(buf, sizeof(buf), "%d", d->set_washing.drain_time_min);
        lv_textarea_set_text(w->ta_drain_time, buf);
    }

    /* Timeouts */
    if (strlen(lv_textarea_get_text(w->ta_pump_confirm)) == 0) {
        snprintf(buf, sizeof(buf), "%d", d->set_timeouts.pump_confirm_ms);
        lv_textarea_set_text(w->ta_pump_confirm, buf);
    }
    if (strlen(lv_textarea_get_text(w->ta_pump_ramp)) == 0) {
        snprintf(buf, sizeof(buf), "%d", d->set_timeouts.pump_ramp_ms);
        lv_textarea_set_text(w->ta_pump_ramp, buf);
    }
}
