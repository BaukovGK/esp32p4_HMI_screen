/*
 * scr_washing.c -- Washing control screen
 *
 * 7-phase stepper, temperature display, CONFIRM + STOP buttons.
 *
 * Content area: 1280 x 700 px.
 */

#include "scr_washing.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_events.h"
#include "ui_fonts.h"
#include "lang.h"
#include <stdio.h>
#include <math.h>

/* ---- Phase definitions ---- */

#define WASH_PHASE_COUNT 7

static const str_id_t phase_str_ids[WASH_PHASE_COUNT] = {
    STR_WASH_WAIT_HEAT,
    STR_WASH_HEATING,
    STR_WASH_WAIT_SUPPLY,
    STR_WASH_SUPPLY,
    STR_WASH_WAIT_DRAIN,
    STR_WASH_DRAIN,
    STR_WASH_DONE,
};

/* Map wash_sub_state_t enum values to 0-based phase index (NONE maps to -1) */
static int wash_sub_to_index(wash_sub_state_t ws)
{
    switch (ws) {
        case WASH_SUB_WAIT_HEAT:   return 0;
        case WASH_SUB_HEATING:     return 1;
        case WASH_SUB_WAIT_SUPPLY: return 2;
        case WASH_SUB_SUPPLY:      return 3;
        case WASH_SUB_WAIT_DRAIN:  return 4;
        case WASH_SUB_DRAIN:       return 5;
        case WASH_SUB_DONE:        return 6;
        default:                   return -1;
    }
}

static bool is_wait_phase(wash_sub_state_t ws)
{
    return ws == WASH_SUB_WAIT_HEAT ||
           ws == WASH_SUB_WAIT_SUPPLY ||
           ws == WASH_SUB_WAIT_DRAIN;
}

/* ---- Widget storage ---- */

typedef struct {
    lv_obj_t *phase_circle[WASH_PHASE_COUNT];
    lv_obj_t *phase_label[WASH_PHASE_COUNT];
    lv_obj_t *phase_connector[WASH_PHASE_COUNT - 1]; /* lines between circles */

    lv_obj_t *lbl_temp_current;
    lv_obj_t *lbl_temp_target;
    lv_obj_t *lbl_phase_name;

    lv_obj_t *btn_start;
    lv_obj_t *btn_confirm;
    lv_obj_t *btn_stop;
} washing_widgets_t;

/* ---- Create ---- */

lv_obj_t *scr_washing_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 1280, 700);
    lv_obj_set_style_bg_color(cont, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    washing_widgets_t *w = lv_malloc(sizeof(washing_widgets_t));
    lv_memzero(w, sizeof(washing_widgets_t));

    /* ---- Phase stepper (horizontal row, full width) ---- */
    int32_t circle_d = 56;
    /* 7 circles spanning 1280px with 40px padding each side */
    /* First circle left edge at 40, last at 1280-40-56=1184 */
    /* spacing = (1184 - 40) / 6 = 190 */
    int32_t start_x  = 40;
    int32_t spacing   = 190;
    int32_t step_y   = 80;

    for (int i = 0; i < WASH_PHASE_COUNT; i++) {
        int32_t cx = start_x + i * spacing;

        /* Circle */
        lv_obj_t *circle = lv_obj_create(cont);
        lv_obj_remove_style_all(circle);
        lv_obj_set_size(circle, circle_d, circle_d);
        lv_obj_set_pos(circle, cx, step_y);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(circle, COLOR_BG_WIDGET, 0);
        lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(circle, COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_border_width(circle, 2, 0);
        lv_obj_remove_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

        /* Number inside circle */
        lv_obj_t *num = lv_label_create(circle);
        char nb[4];
        snprintf(nb, sizeof(nb), "%d", i + 1);
        lv_label_set_text(num, nb);
        lv_obj_set_style_text_color(num, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(num, UI_FONT_20, 0);
        lv_obj_center(num);

        w->phase_circle[i] = circle;

        /* Label below circle (centered on circle) */
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, lang_str(phase_str_ids[i]));
        lv_obj_set_width(lbl, spacing - 10);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lbl, cx - (spacing - circle_d) / 2, step_y + circle_d + 8);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
        w->phase_label[i] = lbl;

        /* Connector line to next circle */
        if (i < WASH_PHASE_COUNT - 1) {
            lv_obj_t *line = lv_obj_create(cont);
            lv_obj_remove_style_all(line);
            lv_obj_set_size(line, spacing - circle_d, 3);
            lv_obj_set_pos(line, cx + circle_d, step_y + circle_d / 2 - 1);
            lv_obj_set_style_bg_color(line, COLOR_TEXT_SECONDARY, 0);
            lv_obj_set_style_bg_opa(line, LV_OPA_60, 0);
            lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);
            w->phase_connector[i] = line;
        }
    }

    /* ---- Current phase name (large) ---- */
    w->lbl_phase_name = lv_label_create(cont);
    lv_label_set_text(w->lbl_phase_name, "---");
    lv_obj_set_pos(w->lbl_phase_name, 60, 240);
    lv_obj_set_style_text_color(w->lbl_phase_name, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(w->lbl_phase_name, UI_FONT_28, 0);

    /* ---- Temperature panel ---- */
    lv_obj_t *temp_panel = lv_obj_create(cont);
    lv_obj_remove_style_all(temp_panel);
    lv_obj_set_size(temp_panel, 500, 140);
    lv_obj_set_pos(temp_panel, 60, 300);
    lv_obj_set_style_bg_color(temp_panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(temp_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(temp_panel, 8, 0);
    lv_obj_set_style_pad_all(temp_panel, 20, 0);
    lv_obj_remove_flag(temp_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *temp_title = lv_label_create(temp_panel);
    lv_label_set_text(temp_title, lang_str(STR_LBL_TEMPERATURE));
    lv_obj_set_pos(temp_title, 0, 0);
    lv_obj_set_style_text_color(temp_title, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(temp_title, UI_FONT_16, 0);

    /* "current / target  C" */
    w->lbl_temp_current = lv_label_create(temp_panel);
    lv_label_set_text(w->lbl_temp_current, "--- / --- \xc2\xb0\x43");
    lv_obj_set_pos(w->lbl_temp_current, 0, 36);
    lv_obj_set_style_text_color(w->lbl_temp_current, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(w->lbl_temp_current, UI_FONT_36, 0);

    /* Small subtitle */
    w->lbl_temp_target = lv_label_create(temp_panel);
    lv_label_set_text(w->lbl_temp_target, "");
    lv_obj_set_pos(w->lbl_temp_target, 0, 86);
    lv_obj_set_style_text_color(w->lbl_temp_target, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(w->lbl_temp_target, UI_FONT_14, 0);

    /* ---- Buttons (centered, 100px from bottom) ---- */
    int32_t btn_w = 300;
    int32_t btn_h = 64;
    int32_t btn_gap = 24;
    /* 3 buttons: 3*300 + 2*24 = 948, margin = (1280-948)/2 = 166 */
    int32_t btn_x0 = 166;
    int32_t btn_y = 700 - 100 - btn_h;

    /* START WASHING button (visible when NOT in washing state) */
    w->btn_start = lv_button_create(cont);
    lv_obj_set_size(w->btn_start, btn_w, btn_h);
    lv_obj_set_pos(w->btn_start, btn_x0, btn_y);
    lv_obj_set_style_bg_color(w->btn_start, COLOR_STATE_WASHING, 0);
    lv_obj_set_style_bg_opa(w->btn_start, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(w->btn_start, 10, 0);
    lv_obj_add_event_cb(w->btn_start, ui_evt_start_washing, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_start = lv_label_create(w->btn_start);
    lv_label_set_text(lbl_start, lang_str(STR_BTN_START_WASHING));
    lv_obj_set_style_text_color(lbl_start, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_start, UI_FONT_20, 0);
    lv_obj_center(lbl_start);

    /* CONFIRM button (visible only in WAIT_* phases) */
    w->btn_confirm = lv_button_create(cont);
    lv_obj_set_size(w->btn_confirm, btn_w, btn_h);
    lv_obj_set_pos(w->btn_confirm, btn_x0 + (btn_w + btn_gap), btn_y);
    lv_obj_set_style_bg_color(w->btn_confirm, COLOR_BTN_SUCCESS, 0);
    lv_obj_set_style_bg_opa(w->btn_confirm, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(w->btn_confirm, 10, 0);
    lv_obj_add_event_cb(w->btn_confirm, ui_evt_confirm_wash, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_confirm = lv_label_create(w->btn_confirm);
    lv_label_set_text(lbl_confirm, lang_str(STR_BTN_CONFIRM));
    lv_obj_set_style_text_color(lbl_confirm, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_confirm, UI_FONT_20, 0);
    lv_obj_center(lbl_confirm);

    lv_obj_add_flag(w->btn_confirm, LV_OBJ_FLAG_HIDDEN); /* hidden by default */

    /* STOP button */
    w->btn_stop = lv_button_create(cont);
    lv_obj_set_size(w->btn_stop, btn_w, btn_h);
    lv_obj_set_pos(w->btn_stop, btn_x0 + 2 * (btn_w + btn_gap), btn_y);
    lv_obj_set_style_bg_color(w->btn_stop, COLOR_BTN_DANGER, 0);
    lv_obj_set_style_bg_opa(w->btn_stop, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(w->btn_stop, 10, 0);
    lv_obj_add_event_cb(w->btn_stop, ui_evt_stop, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_stop = lv_label_create(w->btn_stop);
    lv_label_set_text(lbl_stop, lang_str(STR_BTN_STOP));
    lv_obj_set_style_text_color(lbl_stop, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_stop, UI_FONT_20, 0);
    lv_obj_center(lbl_stop);

    lv_obj_set_user_data(cont, w);
    return cont;
}

/* ---- Update ---- */

void scr_washing_update(lv_obj_t *container, const plant_data_t *d)
{
    washing_widgets_t *w = (washing_widgets_t *)lv_obj_get_user_data(container);
    if (!w) return;

    int active_idx = wash_sub_to_index(d->wash_sub);

    /* Update stepper circles */
    for (int i = 0; i < WASH_PHASE_COUNT; i++) {
        lv_color_t bg;
        lv_color_t border;

        if (i < active_idx) {
            /* Completed phase */
            bg     = COLOR_BTN_SUCCESS;
            border = COLOR_BTN_SUCCESS;
        } else if (i == active_idx) {
            /* Current phase */
            bg     = COLOR_ACCENT;
            border = COLOR_ACCENT;
        } else {
            /* Future phase */
            bg     = COLOR_BG_WIDGET;
            border = COLOR_TEXT_SECONDARY;
        }

        lv_obj_set_style_bg_color(w->phase_circle[i], bg, 0);
        lv_obj_set_style_border_color(w->phase_circle[i], border, 0);

        /* Highlight current label */
        lv_obj_set_style_text_color(w->phase_label[i],
            (i == active_idx) ? COLOR_ACCENT : COLOR_TEXT_SECONDARY, 0);
    }

    /* Update connector colors */
    for (int i = 0; i < WASH_PHASE_COUNT - 1; i++) {
        lv_color_t c = (i < active_idx) ? COLOR_BTN_SUCCESS : COLOR_TEXT_SECONDARY;
        lv_obj_set_style_bg_color(w->phase_connector[i], c, 0);
        lv_obj_set_style_bg_opa(w->phase_connector[i],
            (i < active_idx) ? LV_OPA_COVER : LV_OPA_60, 0);
    }

    /* Current phase name */
    if (active_idx >= 0 && active_idx < WASH_PHASE_COUNT) {
        lv_label_set_text(w->lbl_phase_name, lang_str(phase_str_ids[active_idx]));
    } else {
        lv_label_set_text(w->lbl_phase_name, lang_str(STR_MODE_IDLE));
    }

    /* Temperature display: "current / target  C" */
    char buf[64];
    float t_cur = d->temperature.fault ? NAN : d->temperature.value;
    float t_tgt = d->set_washing.target_temp_C;

    if (isnan(t_cur)) {
        snprintf(buf, sizeof(buf), "--- / %.1f \xc2\xb0\x43", (double)t_tgt);
    } else {
        snprintf(buf, sizeof(buf), "%.1f / %.1f \xc2\xb0\x43",
                 (double)t_cur, (double)t_tgt);
    }
    lv_label_set_text(w->lbl_temp_current, buf);

    /* Color temperature red if above max */
    if (!isnan(t_cur) && t_cur > d->set_washing.max_temp_C) {
        lv_obj_set_style_text_color(w->lbl_temp_current, COLOR_PUMP_FAULT, 0);
    } else {
        lv_obj_set_style_text_color(w->lbl_temp_current, COLOR_TEXT_VALUE, 0);
    }

    /* Target info */
    snprintf(buf, sizeof(buf), "Target: %.1f \xc2\xb0\x43  |  Max: %.1f \xc2\xb0\x43",
             (double)d->set_washing.target_temp_C,
             (double)d->set_washing.max_temp_C);
    lv_label_set_text(w->lbl_temp_target, buf);

    /* Button visibility */
    if (d->state == PLANT_STATE_WASHING) {
        /* During washing: hide START, show STOP, show CONFIRM only in WAIT phases */
        lv_obj_add_flag(w->btn_start, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(w->btn_stop, LV_OBJ_FLAG_HIDDEN);
        if (is_wait_phase(d->wash_sub)) {
            lv_obj_remove_flag(w->btn_confirm, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(w->btn_confirm, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        /* Not washing: show START, hide CONFIRM and STOP */
        lv_obj_remove_flag(w->btn_start, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(w->btn_confirm, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(w->btn_stop, LV_OBJ_FLAG_HIDDEN);
    }
}
