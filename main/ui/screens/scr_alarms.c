/*
 * scr_alarms.c -- Alarms screen
 *
 * Active alarms (top), history journal (bottom), RESET FAULT button.
 *
 * Content area: 1280 x 700 px.
 */

#include "scr_alarms.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_events.h"
#include "ui_fonts.h"
#include "lang.h"
#include "alarm_ring.h"
#include <stdio.h>
#include <math.h>
#include <time.h>

/* Maximum entries we'll display */
#define MAX_ACTIVE  ALARM_RING_SIZE
#define MAX_HISTORY ALARM_RING_SIZE

/* ---- Widget storage ---- */

typedef struct {
    lv_obj_t *active_list;      /* scrollable container for active alarms */
    lv_obj_t *history_list;     /* scrollable container for history */
    lv_obj_t *lbl_active_title;
    lv_obj_t *lbl_history_title;
    lv_obj_t *btn_reset;
} alarms_widgets_t;

/* ---- Helpers ---- */

static lv_color_t alarm_cat_color(alarm_category_t cat)
{
    switch (cat) {
        case ALARM_CAT_CRITICAL: return COLOR_ALARM_CRITICAL;
        case ALARM_CAT_ALARM:    return COLOR_ALARM_ALARM;
        case ALARM_CAT_WARNING:  return COLOR_ALARM_WARNING;
        case ALARM_CAT_INFO:     return COLOR_ALARM_INFO;
        default:                 return COLOR_TEXT_SECONDARY;
    }
}

static const char *alarm_cat_icon(alarm_category_t cat)
{
    switch (cat) {
        case ALARM_CAT_CRITICAL: return LV_SYMBOL_WARNING;
        case ALARM_CAT_ALARM:    return LV_SYMBOL_WARNING;
        case ALARM_CAT_WARNING:  return LV_SYMBOL_BELL;
        case ALARM_CAT_INFO:     return LV_SYMBOL_EYE_OPEN;
        default:                 return "?";
    }
}

static void format_timestamp(char *buf, size_t len, int64_t ts_sec)
{
    if (ts_sec <= 0) {
        snprintf(buf, len, "--:--:--");
        return;
    }
    time_t t = (time_t)ts_sec;
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    snprintf(buf, len, "%02d:%02d:%02d",
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
}

static lv_obj_t *make_alarm_row(lv_obj_t *parent, const alarm_entry_t *entry)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), 36);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_style_pad_left(row, 8, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Left color bar (category indicator) */
    lv_obj_t *bar = lv_obj_create(row);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 6, 28);
    lv_obj_set_style_bg_color(bar, alarm_cat_color(entry->cat), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 3, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Icon */
    lv_obj_t *icon = lv_label_create(row);
    lv_label_set_text(icon, alarm_cat_icon(entry->cat));
    lv_obj_set_width(icon, 30);
    lv_obj_set_style_text_color(icon, alarm_cat_color(entry->cat), 0);
    lv_obj_set_style_text_font(icon, UI_FONT_16, 0);

    /* Code */
    lv_obj_t *code = lv_label_create(row);
    lv_label_set_text(code, entry->code);
    lv_obj_set_width(code, 200);
    lv_obj_set_style_text_color(code, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(code, UI_FONT_14, 0);

    /* Value */
    char val_buf[32];
    if (isnan(entry->value)) {
        snprintf(val_buf, sizeof(val_buf), "---");
    } else {
        snprintf(val_buf, sizeof(val_buf), "%.2f", (double)entry->value);
    }
    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, val_buf);
    lv_obj_set_width(val, 120);
    lv_obj_set_style_text_color(val, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(val, UI_FONT_14, 0);

    /* Timestamp */
    char ts_buf[16];
    format_timestamp(ts_buf, sizeof(ts_buf), entry->ts);
    lv_obj_t *ts = lv_label_create(row);
    lv_label_set_text(ts, ts_buf);
    lv_obj_set_width(ts, 100);
    lv_obj_set_style_text_color(ts, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(ts, UI_FONT_14, 0);

    /* Active indicator */
    lv_obj_t *act = lv_label_create(row);
    lv_label_set_text(act, entry->active ? "ACTIVE" : "");
    lv_obj_set_width(act, 80);
    lv_obj_set_style_text_color(act, COLOR_PUMP_FAULT, 0);
    lv_obj_set_style_text_font(act, UI_FONT_12, 0);

    return row;
}

/* ---- Create ---- */

lv_obj_t *scr_alarms_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 1280, 700);
    lv_obj_set_style_bg_color(cont, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    alarms_widgets_t *w = lv_malloc(sizeof(alarms_widgets_t));
    lv_memzero(w, sizeof(alarms_widgets_t));

    /* ---- Active alarms section (top half) ---- */
    w->lbl_active_title = lv_label_create(cont);
    lv_label_set_text(w->lbl_active_title, "Active Alarms (0)");
    lv_obj_set_pos(w->lbl_active_title, 20, 8);
    lv_obj_set_style_text_color(w->lbl_active_title, COLOR_ALARM_ALARM, 0);
    lv_obj_set_style_text_font(w->lbl_active_title, UI_FONT_18, 0);

    w->active_list = lv_obj_create(cont);
    lv_obj_remove_style_all(w->active_list);
    lv_obj_set_size(w->active_list, 1240, 240);
    lv_obj_set_pos(w->active_list, 20, 36);
    lv_obj_set_style_bg_color(w->active_list, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(w->active_list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(w->active_list, 8, 0);
    lv_obj_set_style_pad_all(w->active_list, 8, 0);
    lv_obj_set_layout(w->active_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(w->active_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(w->active_list, 2, 0);
    lv_obj_add_flag(w->active_list, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- History section (bottom half) ---- */
    w->lbl_history_title = lv_label_create(cont);
    lv_label_set_text(w->lbl_history_title, "Alarm History");
    lv_obj_set_pos(w->lbl_history_title, 20, 290);
    lv_obj_set_style_text_color(w->lbl_history_title, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(w->lbl_history_title, UI_FONT_18, 0);

    w->history_list = lv_obj_create(cont);
    lv_obj_remove_style_all(w->history_list);
    lv_obj_set_size(w->history_list, 1240, 290);
    lv_obj_set_pos(w->history_list, 20, 318);
    lv_obj_set_style_bg_color(w->history_list, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(w->history_list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(w->history_list, 8, 0);
    lv_obj_set_style_pad_all(w->history_list, 8, 0);
    lv_obj_set_layout(w->history_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(w->history_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(w->history_list, 2, 0);
    lv_obj_add_flag(w->history_list, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- RESET FAULT button ---- */
    w->btn_reset = lv_button_create(cont);
    lv_obj_set_size(w->btn_reset, 220, 54);
    lv_obj_set_pos(w->btn_reset, 20, 630);
    lv_obj_set_style_bg_color(w->btn_reset, COLOR_BTN_DANGER, 0);
    lv_obj_set_style_bg_opa(w->btn_reset, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(w->btn_reset, 8, 0);
    lv_obj_add_event_cb(w->btn_reset, ui_evt_reset_fault, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_reset = lv_label_create(w->btn_reset);
    lv_label_set_text(lbl_reset, lang_str(STR_BTN_RESET_FAULT));
    lv_obj_set_style_text_color(lbl_reset, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_reset, UI_FONT_16, 0);
    lv_obj_center(lbl_reset);

    lv_obj_add_flag(w->btn_reset, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_user_data(cont, w);
    return cont;
}

/* ---- Update ---- */

void scr_alarms_update(lv_obj_t *container, const plant_data_t *d)
{
    alarms_widgets_t *w = (alarms_widgets_t *)lv_obj_get_user_data(container);
    if (!w) return;

    /* ---- Active alarms ---- */
    alarm_entry_t active[MAX_ACTIVE];
    int n_active = alarm_ring_get_active(active, MAX_ACTIVE);

    char title_buf[48];
    snprintf(title_buf, sizeof(title_buf), "Active Alarms (%d)", n_active);
    lv_label_set_text(w->lbl_active_title, title_buf);

    /* Rebuild active list */
    lv_obj_clean(w->active_list);
    for (int i = 0; i < n_active; i++) {
        make_alarm_row(w->active_list, &active[i]);
    }
    if (n_active == 0) {
        lv_obj_t *empty = lv_label_create(w->active_list);
        lv_label_set_text(empty, "No active alarms");
        lv_obj_set_style_text_color(empty, COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(empty, UI_FONT_16, 0);
    }

    /* ---- History ---- */
    alarm_entry_t history[MAX_HISTORY];
    int n_history = alarm_ring_get_history(history, MAX_HISTORY);

    lv_obj_clean(w->history_list);
    for (int i = 0; i < n_history; i++) {
        make_alarm_row(w->history_list, &history[i]);
    }
    if (n_history == 0) {
        lv_obj_t *empty = lv_label_create(w->history_list);
        lv_label_set_text(empty, "No alarm history");
        lv_obj_set_style_text_color(empty, COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(empty, UI_FONT_16, 0);
    }

    /* ---- RESET button visibility ---- */
    if (d->state == PLANT_STATE_FAULT) {
        lv_obj_remove_flag(w->btn_reset, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(w->btn_reset, LV_OBJ_FLAG_HIDDEN);
    }
}
