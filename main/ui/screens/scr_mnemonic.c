/*
 * scr_mnemonic.c -- Main P&ID mnemonic screen
 *
 * Process flow (left to right):
 *   Source Tank → Feed Pump → Stage1 Pump → [Membrane 1] →
 *   Intermediate Tank → Stage2 Pump → [Membrane 2] → Clean Water Tank
 *
 * Content area: 1280 x 700 px.
 */

#include "scr_mnemonic.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_events.h"
#include "ui_fonts.h"
#include "lang.h"
#include <stdio.h>
#include <math.h>

/* ---- DI / DO bit definitions ---- */

/* Digital inputs (di byte) */
#define DI_SOURCE_LOW   (1u << 0)   /* DI1: source tank low level   */
#define DI_SOURCE_HIGH  (1u << 1)   /* DI2: source tank high level  */
#define DI_INTERM_LOW   (1u << 2)   /* DI3: intermediate tank low   */
#define DI_INTERM_HIGH  (1u << 3)   /* DI4: intermediate tank high  */
#define DI_PUMP1_RUN    (1u << 4)   /* DI5: feed pump confirm       */
#define DI_PUMP2_RUN    (1u << 5)   /* DI6: stage1 pump confirm     */
#define DI_PUMP3_RUN    (1u << 6)   /* DI7: stage2 pump confirm     */
#define DI_PERMEATE_HIGH (1u << 7)  /* DI8: permeate tank high      */

/* Digital outputs (do_bits byte) */
#define DO_PUMP1        (1u << 0)
#define DO_PUMP2        (1u << 1)
#define DO_PUMP3        (1u << 2)
#define DO_HEATER       (1u << 3)
#define DO_DOSER        (1u << 4)
#define DO_VALVE1       (1u << 5)
#define DO_VALVE2       (1u << 6)

/* ---- Schema layout constants ---- */

#define PIPE_Y      250     /* Centerline of horizontal pipes */
#define TANK_TOP    115     /* Top of tank rectangles */
#define TANK_W      110     /* Tank width */
#define TANK_H      250     /* Tank height */
#define PUMP_D       60     /* Pump circle diameter */
#define MEMB_W       70     /* Membrane block width */
#define MEMB_H      110     /* Membrane block height */

/* X positions of process elements (left to right, filling 1280px) */
#define X_SRC        15     /* Source tank */
#define X_FEED      210     /* Feed pump */
#define X_STG1      355     /* Stage 1 pump */
#define X_M1        500     /* Membrane 1 */
#define X_INT       655     /* Intermediate tank */
#define X_STG2      850     /* Stage 2 pump */
#define X_M2        995     /* Membrane 2 */
#define X_CLN      1150     /* Clean water tank → right edge at 1260 */

/* ---- Widget storage (user_data on container) ---- */

typedef struct {
    /* Tanks: fill rectangle inside an outline */
    lv_obj_t *tank_source;
    lv_obj_t *tank_source_fill;
    lv_obj_t *tank_interm;
    lv_obj_t *tank_interm_fill;
    lv_obj_t *tank_permeate;
    lv_obj_t *tank_permeate_fill;

    /* Equipment indicators (circles) */
    lv_obj_t *ind_pump1;
    lv_obj_t *ind_pump2;
    lv_obj_t *ind_pump3;
    lv_obj_t *ind_heater;
    lv_obj_t *ind_doser;

    /* Sensor value labels */
    lv_obj_t *lbl_p[4];
    lv_obj_t *lbl_t;
    lv_obj_t *lbl_q[4];
    lv_obj_t *lbl_s[3];

    /* Telemetry */
    lv_obj_t *lbl_recovery;
    lv_obj_t *lbl_filter_dp;

    /* Mode / state label */
    lv_obj_t *lbl_state;

    /* Control buttons */
    lv_obj_t *btn_start;
    lv_obj_t *btn_stop;
    lv_obj_t *btn_manual;
    lv_obj_t *btn_washing;
    lv_obj_t *btn_reset;
} mnemonic_widgets_t;

/* ---- Helpers ---- */

static lv_obj_t *make_tank(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t w, int32_t h, const char *title,
                           lv_obj_t **fill_out)
{
    /* Outer rectangle (border only) */
    lv_obj_t *frame = lv_obj_create(parent);
    lv_obj_remove_style_all(frame);
    lv_obj_set_size(frame, w, h);
    lv_obj_set_pos(frame, x, y);
    lv_obj_set_style_border_color(frame, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_border_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_20, 0);
    lv_obj_set_style_bg_color(frame, COLOR_BG_WIDGET, 0);
    lv_obj_set_style_radius(frame, 4, 0);
    lv_obj_set_style_pad_all(frame, 0, 0);
    lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    /* Fill indicator (grows from bottom) */
    lv_obj_t *fill = lv_obj_create(frame);
    lv_obj_remove_style_all(fill);
    lv_obj_set_size(fill, w - 4, 0);
    lv_obj_set_align(fill, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_color(fill, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_60, 0);
    lv_obj_set_style_radius(fill, 2, 0);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    *fill_out = fill;

    /* Title label below tank */
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
    lv_obj_set_pos(lbl, x, y + h + 4);

    return frame;
}

static lv_obj_t *make_indicator(lv_obj_t *parent, int32_t x, int32_t y,
                                int32_t diameter, const char *title)
{
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, diameter, diameter);
    lv_obj_set_pos(circle, x, y);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, COLOR_PUMP_OFF, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(circle, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_border_width(circle, 2, 0);
    lv_obj_remove_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    /* Title label below */
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_12, 0);
    lv_obj_set_pos(lbl, x, y + diameter + 2);

    return circle;
}

static void make_pipe(lv_obj_t *parent, int32_t x, int32_t length)
{
    lv_obj_t *pipe = lv_obj_create(parent);
    lv_obj_remove_style_all(pipe);
    lv_obj_set_size(pipe, length, 3);
    lv_obj_set_pos(pipe, x, PIPE_Y - 1);
    lv_obj_set_style_bg_color(pipe, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_bg_opa(pipe, LV_OPA_60, 0);
    lv_obj_remove_flag(pipe, LV_OBJ_FLAG_SCROLLABLE);
}

static void make_membrane(lv_obj_t *parent, int32_t x,
                          const char *short_name, const char *title)
{
    int32_t my = PIPE_Y - MEMB_H / 2;

    lv_obj_t *memb = lv_obj_create(parent);
    lv_obj_remove_style_all(memb);
    lv_obj_set_size(memb, MEMB_W, MEMB_H);
    lv_obj_set_pos(memb, x, my);
    lv_obj_set_style_bg_color(memb, lv_color_hex(0x1A3A5C), 0);
    lv_obj_set_style_bg_opa(memb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(memb, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(memb, 2, 0);
    lv_obj_set_style_radius(memb, 6, 0);
    lv_obj_remove_flag(memb, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(memb);
    lv_label_set_text(lbl, short_name);
    lv_obj_set_style_text_color(lbl, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_18, 0);
    lv_obj_center(lbl);

    lv_obj_t *tlbl = lv_label_create(parent);
    lv_label_set_text(tlbl, title);
    lv_obj_set_pos(tlbl, x, my + MEMB_H + 4);
    lv_obj_set_style_text_color(tlbl, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(tlbl, UI_FONT_12, 0);
}

static lv_obj_t *make_sensor_label(lv_obj_t *parent, int32_t x, int32_t y,
                                   const char *prefix)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, prefix);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_16, 0);
    return lbl;
}

static lv_obj_t *make_mode_btn(lv_obj_t *parent, int32_t x, int32_t y,
                               int32_t w, int32_t h,
                               const char *text, lv_color_t bg,
                               lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 6, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_16, 0);
    lv_obj_center(lbl);

    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    }
    return btn;
}

/* ---- Create ---- */

lv_obj_t *scr_mnemonic_create(lv_obj_t *parent)
{
    /* Root container - fills content area */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 1280, 700);
    lv_obj_set_style_bg_color(cont, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    mnemonic_widgets_t *w = lv_malloc(sizeof(mnemonic_widgets_t));
    lv_memzero(w, sizeof(mnemonic_widgets_t));

    /*
     * ============================================================
     *  PROCESS SCHEMA  (y = 15 .. 280)
     *
     *  [Source] → (Feed) → (Stage1) → |M1| → [Interm] → (Stage2) → |M2| → [Clean]
     * ============================================================
     */

    /* ---- Tanks ---- */
    w->tank_source = make_tank(cont, X_SRC, TANK_TOP, TANK_W, TANK_H,
                               lang_str(STR_LBL_SOURCE_TANK), &w->tank_source_fill);
    w->tank_interm = make_tank(cont, X_INT, TANK_TOP, TANK_W, TANK_H,
                               lang_str(STR_LBL_INTERM_TANK), &w->tank_interm_fill);
    w->tank_permeate = make_tank(cont, X_CLN, TANK_TOP, TANK_W, TANK_H,
                                 lang_str(STR_LBL_PERMEATE_TANK), &w->tank_permeate_fill);

    /* ---- Pumps ---- */
    w->ind_pump1 = make_indicator(cont, X_FEED, PIPE_Y - PUMP_D / 2, PUMP_D,
                                  lang_str(STR_LBL_FEED_PUMP));
    w->ind_pump2 = make_indicator(cont, X_STG1, PIPE_Y - PUMP_D / 2, PUMP_D,
                                  lang_str(STR_LBL_STAGE1_PUMP));
    w->ind_pump3 = make_indicator(cont, X_STG2, PIPE_Y - PUMP_D / 2, PUMP_D,
                                  lang_str(STR_LBL_STAGE2_PUMP));

    /* ---- Membranes ---- */
    make_membrane(cont, X_M1, "M1", lang_str(STR_LBL_MEMBRANE_1));
    make_membrane(cont, X_M2, "M2", lang_str(STR_LBL_MEMBRANE_2));

    /* ---- Heater & Doser (below pipe, near intermediate area) ---- */
    w->ind_heater = make_indicator(cont, X_INT + TANK_W + 15,
                                   PIPE_Y + PUMP_D / 2 + 20, 45,
                                   lang_str(STR_LBL_HEATER));
    w->ind_doser  = make_indicator(cont, X_FEED + PUMP_D + 15,
                                   PIPE_Y + PUMP_D / 2 + 20, 45,
                                   lang_str(STR_LBL_DOSER));

    /* ---- Connecting pipes (at PIPE_Y centerline) ---- */
    make_pipe(cont, X_SRC + TANK_W,     X_FEED - (X_SRC + TANK_W));     /* Source → Feed   */
    make_pipe(cont, X_FEED + PUMP_D,    X_STG1 - (X_FEED + PUMP_D));    /* Feed → Stage1   */
    make_pipe(cont, X_STG1 + PUMP_D,    X_M1 - (X_STG1 + PUMP_D));     /* Stage1 → M1     */
    make_pipe(cont, X_M1 + MEMB_W,      X_INT - (X_M1 + MEMB_W));      /* M1 → Interm     */
    make_pipe(cont, X_INT + TANK_W,     X_STG2 - (X_INT + TANK_W));     /* Interm → Stage2 */
    make_pipe(cont, X_STG2 + PUMP_D,    X_M2 - (X_STG2 + PUMP_D));     /* Stage2 → M2     */
    make_pipe(cont, X_M2 + MEMB_W,      X_CLN - (X_M2 + MEMB_W));      /* M2 → Clean      */

    /* ---- Pressure labels P1-P4 (above pipe, near measurement points) ---- */
    w->lbl_p[0] = make_sensor_label(cont, X_FEED, PIPE_Y - PUMP_D / 2 - 22,  "P1: ---");
    w->lbl_p[1] = make_sensor_label(cont, X_M1,   PIPE_Y - MEMB_H / 2 - 22,  "P2: ---");
    w->lbl_p[2] = make_sensor_label(cont, X_STG2,  PIPE_Y - PUMP_D / 2 - 22,  "P3: ---");
    w->lbl_p[3] = make_sensor_label(cont, X_M2,   PIPE_Y - MEMB_H / 2 - 22,  "P4: ---");

    /*
     * ============================================================
     *  SENSOR VALUES  (y = 295 .. 370)
     * ============================================================
     */

    /* Temperature */
    w->lbl_t = make_sensor_label(cont, 30, 400, "T: ---");

    /* Flow Q1..Q4 */
    w->lbl_q[0] = make_sensor_label(cont, 220, 400, "Q1: ---");
    w->lbl_q[1] = make_sensor_label(cont, 440, 400, "Q2: ---");
    w->lbl_q[2] = make_sensor_label(cont, 700, 400, "Q3: ---");
    w->lbl_q[3] = make_sensor_label(cont, 960, 400, "Q4: ---");

    /* Conductivity S1..S3 */
    w->lbl_s[0] = make_sensor_label(cont, 30,  435, "S1: ---");
    w->lbl_s[1] = make_sensor_label(cont, 440, 435, "S2: ---");
    w->lbl_s[2] = make_sensor_label(cont, 850, 435, "S3: ---");

    /*
     * ============================================================
     *  TELEMETRY  (y = 480)
     * ============================================================
     */

    lv_obj_t *tel_panel = lv_obj_create(cont);
    lv_obj_remove_style_all(tel_panel);
    lv_obj_set_size(tel_panel, 620, 80);
    lv_obj_set_pos(tel_panel, 30, 480);
    lv_obj_set_style_bg_color(tel_panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(tel_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tel_panel, 8, 0);
    lv_obj_set_style_pad_all(tel_panel, 12, 0);
    lv_obj_remove_flag(tel_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Recovery */
    lv_obj_t *rec_title = lv_label_create(tel_panel);
    lv_label_set_text(rec_title, lang_str(STR_LBL_RECOVERY));
    lv_obj_set_style_text_color(rec_title, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(rec_title, UI_FONT_14, 0);
    lv_obj_set_pos(rec_title, 0, 0);

    w->lbl_recovery = lv_label_create(tel_panel);
    lv_label_set_text(w->lbl_recovery, "--- %");
    lv_obj_set_style_text_color(w->lbl_recovery, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(w->lbl_recovery, UI_FONT_28, 0);
    lv_obj_set_pos(w->lbl_recovery, 0, 22);

    /* Filter DP */
    lv_obj_t *dp_title = lv_label_create(tel_panel);
    lv_label_set_text(dp_title, lang_str(STR_LBL_FILTER_DP));
    lv_obj_set_style_text_color(dp_title, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(dp_title, UI_FONT_14, 0);
    lv_obj_set_pos(dp_title, 310, 0);

    w->lbl_filter_dp = lv_label_create(tel_panel);
    lv_label_set_text(w->lbl_filter_dp, "--- bar");
    lv_obj_set_style_text_color(w->lbl_filter_dp, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(w->lbl_filter_dp, UI_FONT_22, 0);
    lv_obj_set_pos(w->lbl_filter_dp, 310, 22);

    /* ---- State label (right of telemetry panel) ---- */
    w->lbl_state = lv_label_create(cont);
    lv_label_set_text(w->lbl_state, "---");
    lv_obj_set_pos(w->lbl_state, 700, 500);
    lv_obj_set_style_text_color(w->lbl_state, COLOR_STATE_IDLE, 0);
    lv_obj_set_style_text_font(w->lbl_state, UI_FONT_28, 0);

    /*
     * ============================================================
     *  CONTROL BUTTONS  (full width, centered, 50px from bottom)
     * ============================================================
     */

    int32_t btn_h = 54;
    int32_t btn_gap = 16;
    int32_t btn_w = 230;
    /* 5 buttons: 5*230 + 4*16 = 1214, margin = (1280-1214)/2 = 33 */
    int32_t btn_x0 = 33;
    int32_t btn_y = 700 - 50 - btn_h;   /* 50px from bottom edge */

    w->btn_start = make_mode_btn(cont, btn_x0, btn_y, btn_w, btn_h,
                                 lang_str(STR_BTN_START_AUTO),
                                 COLOR_BTN_SUCCESS, ui_evt_start_auto);

    w->btn_stop = make_mode_btn(cont, btn_x0 + (btn_w + btn_gap), btn_y, btn_w, btn_h,
                                lang_str(STR_BTN_STOP),
                                COLOR_BTN_DANGER, ui_evt_stop);

    w->btn_manual = make_mode_btn(cont, btn_x0 + 2 * (btn_w + btn_gap), btn_y, btn_w, btn_h,
                                  lang_str(STR_BTN_MANUAL),
                                  COLOR_STATE_MANUAL, ui_evt_set_manual);

    w->btn_washing = make_mode_btn(cont, btn_x0 + 3 * (btn_w + btn_gap), btn_y, btn_w, btn_h,
                                   lang_str(STR_BTN_START_WASHING),
                                   COLOR_STATE_WASHING, ui_evt_start_washing);

    w->btn_reset = make_mode_btn(cont, btn_x0 + 4 * (btn_w + btn_gap), btn_y, btn_w, btn_h,
                                 lang_str(STR_BTN_RESET_FAULT),
                                 COLOR_ALARM_ALARM, ui_evt_reset_fault);
    lv_obj_add_flag(w->btn_reset, LV_OBJ_FLAG_HIDDEN);

    /* Store widget pointers */
    lv_obj_set_user_data(cont, w);

    return cont;
}

/* ---- Update ---- */

static void set_indicator_color(lv_obj_t *ind, bool do_on, bool di_confirm, bool fault)
{
    lv_color_t c;
    if (fault) {
        c = COLOR_PUMP_FAULT;
    } else if (do_on && di_confirm) {
        c = COLOR_PUMP_RUNNING;
    } else if (do_on && !di_confirm) {
        c = COLOR_PUMP_STARTING;
    } else {
        c = COLOR_PUMP_OFF;
    }
    lv_obj_set_style_bg_color(ind, c, 0);
}

static void set_tank_fill(lv_obj_t *fill, lv_obj_t *frame, bool low, bool high)
{
    /* low=0 high=0 -> empty;  low=1 high=0 -> mid;  low=1 high=1 -> full */
    int pct = 0;
    if (low && high)       pct = 100;
    else if (low && !high) pct = 50;
    else                   pct = 5;   /* minimum visible sliver */

    int32_t tank_h = lv_obj_get_height(frame) - 4;
    int32_t fill_h = (tank_h * pct) / 100;
    lv_obj_set_height(fill, fill_h);
}

void scr_mnemonic_update(lv_obj_t *container, const plant_data_t *d)
{
    mnemonic_widgets_t *w = (mnemonic_widgets_t *)lv_obj_get_user_data(container);
    if (!w) return;

    char buf[48];
    uint8_t di = d->di;
    uint8_t do_bits = d->do_bits;

    /* ---- Tank fill levels ---- */
    set_tank_fill(w->tank_source_fill, w->tank_source,
                  !(di & DI_SOURCE_LOW), (di & DI_SOURCE_HIGH));
    set_tank_fill(w->tank_interm_fill, w->tank_interm,
                  !(di & DI_INTERM_LOW), (di & DI_INTERM_HIGH));
    /* Permeate: only high-level sensor available */
    set_tank_fill(w->tank_permeate_fill, w->tank_permeate,
                  true, (di & DI_PERMEATE_HIGH));

    /* ---- Equipment indicators ---- */
    set_indicator_color(w->ind_pump1, do_bits & DO_PUMP1, di & DI_PUMP1_RUN,
                        d->state == PLANT_STATE_FAULT);
    set_indicator_color(w->ind_pump2, do_bits & DO_PUMP2, di & DI_PUMP2_RUN,
                        d->state == PLANT_STATE_FAULT);
    set_indicator_color(w->ind_pump3, do_bits & DO_PUMP3, di & DI_PUMP3_RUN,
                        d->state == PLANT_STATE_FAULT);

    /* Heater: no DI confirm, just DO state */
    lv_obj_set_style_bg_color(w->ind_heater,
        (do_bits & DO_HEATER) ? COLOR_PUMP_RUNNING : COLOR_PUMP_OFF, 0);

    /* Doser */
    lv_color_t doser_c = COLOR_PUMP_OFF;
    if (d->doser.state == DOSER_STATE_RUNNING) doser_c = COLOR_PUMP_RUNNING;
    else if (d->doser.state == DOSER_STATE_PAUSE) doser_c = COLOR_PUMP_STARTING;
    lv_obj_set_style_bg_color(w->ind_doser, doser_c, 0);

    /* ---- Pressure labels P1..P4 ---- */
    for (int i = 0; i < 4; i++) {
        if (d->pressure[i].fault) {
            snprintf(buf, sizeof(buf), "P%d: FAULT", i + 1);
            lv_label_set_text(w->lbl_p[i], buf);
            lv_obj_set_style_text_color(w->lbl_p[i], COLOR_PUMP_FAULT, 0);
        } else {
            ui_fmt_float(buf, sizeof(buf), d->pressure[i].value, 2,
                         lang_str(STR_UNIT_BAR));
            char full[64];
            snprintf(full, sizeof(full), "P%d: %s", i + 1, buf);
            lv_label_set_text(w->lbl_p[i], full);
            lv_obj_set_style_text_color(w->lbl_p[i], COLOR_TEXT_VALUE, 0);
        }
    }

    /* Temperature */
    if (d->temperature.fault) {
        lv_label_set_text(w->lbl_t, "T: FAULT");
        lv_obj_set_style_text_color(w->lbl_t, COLOR_PUMP_FAULT, 0);
    } else {
        ui_fmt_float(buf, sizeof(buf), d->temperature.value, 1,
                     lang_str(STR_UNIT_CELSIUS));
        char full[64];
        snprintf(full, sizeof(full), "T: %s", buf);
        lv_label_set_text(w->lbl_t, full);
        lv_obj_set_style_text_color(w->lbl_t, COLOR_TEXT_VALUE, 0);
    }

    /* Flow Q1..Q4 */
    for (int i = 0; i < 4; i++) {
        ui_fmt_float(buf, sizeof(buf), d->flow[i].flow, 2,
                     lang_str(STR_UNIT_M3H));
        char full[64];
        snprintf(full, sizeof(full), "Q%d: %s", i + 1, buf);
        lv_label_set_text(w->lbl_q[i], full);
        lv_obj_set_style_text_color(w->lbl_q[i],
            d->flow[i].ok ? COLOR_TEXT_VALUE : COLOR_PUMP_FAULT, 0);
    }

    /* Conductivity S1..S3 */
    for (int i = 0; i < 3; i++) {
        ui_fmt_float(buf, sizeof(buf), d->conductivity[i].conductivity, 1,
                     lang_str(STR_UNIT_USCM));
        char full[64];
        snprintf(full, sizeof(full), "S%d: %s", i + 1, buf);
        lv_label_set_text(w->lbl_s[i], full);
        lv_obj_set_style_text_color(w->lbl_s[i],
            d->conductivity[i].ok ? COLOR_TEXT_VALUE : COLOR_PUMP_FAULT, 0);
    }

    /* ---- Telemetry ---- */
    ui_fmt_float(buf, sizeof(buf), d->telemetry.recovery_sys, 1,
                 lang_str(STR_UNIT_PERCENT));
    lv_label_set_text(w->lbl_recovery, buf);

    ui_fmt_float(buf, sizeof(buf), d->telemetry.filter_dp, 3,
                 lang_str(STR_UNIT_BAR));
    lv_label_set_text(w->lbl_filter_dp, buf);

    /* ---- State label ---- */
    const char *state_text;
    lv_color_t  state_color;
    switch (d->state) {
        case PLANT_STATE_IDLE:    state_text = lang_str(STR_MODE_IDLE);    state_color = COLOR_STATE_IDLE;    break;
        case PLANT_STATE_AUTO:    state_text = lang_str(STR_MODE_AUTO);    state_color = COLOR_STATE_AUTO;    break;
        case PLANT_STATE_WASHING: state_text = lang_str(STR_MODE_WASHING); state_color = COLOR_STATE_WASHING; break;
        case PLANT_STATE_MANUAL:  state_text = lang_str(STR_MODE_MANUAL);  state_color = COLOR_STATE_MANUAL;  break;
        case PLANT_STATE_FAULT:   state_text = lang_str(STR_MODE_FAULT);   state_color = COLOR_STATE_FAULT;   break;
        default:                  state_text = lang_str(STR_MODE_UNKNOWN); state_color = COLOR_STATE_IDLE;    break;
    }
    lv_label_set_text(w->lbl_state, state_text);
    lv_obj_set_style_text_color(w->lbl_state, state_color, 0);

    /* ---- RESET button visibility ---- */
    if (d->state == PLANT_STATE_FAULT) {
        lv_obj_remove_flag(w->btn_reset, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(w->btn_reset, LV_OBJ_FLAG_HIDDEN);
    }
}
