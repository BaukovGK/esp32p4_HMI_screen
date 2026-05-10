/**
 * @file scr_washing.c
 * @brief Экран "Промывка" — переработан под новый дизайн прототипа.
 *
 * Layout (proto/washing.html):
 *   Left card (1fr ≈ 864 px): title + compact 4-phase track + info-strip
 *                              (timers + temperatures в одной полосе) +
 *                              wash-mnemo placeholder (под будущую CIP-схему).
 *   Right column (380 px):    3 карточки:
 *                              - Состояние оборудования (5 строк badge'ей)
 *                              - Действия (4 кнопки управления фазами)
 *                              - Подсказка (текстовая инструкция)
 *
 * 4 фазы (схлопнуто из 7 wash_sub_state_t):
 *   1. Ожидание (NONE, WAIT_HEAT, DONE)
 *   2. Нагрев   (HEATING, WAIT_SUPPLY → done)
 *   3. Подача   (SUPPLY, WAIT_DRAIN → done)
 *   4. Слив     (DRAIN)
 *
 * Каждая фаза отрисовывается как небольшой rect 36 px высотой с
 * номером + названием + duration (только для active). Состояния:
 *   pending → bg_mute, text_secondary
 *   active  → accent_mute, accent border, accent text
 *   done    → success_bg, success border, success text
 *
 * Кнопка confirm_wash активна только в WAIT_* подсостояниях.
 */

#include "scr_washing.h"
#include "ui_tokens.h"
#include "ui_fonts.h"
#include "ui_events.h"
#include "lang.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

/* ─── константы layout'а ──────────────────────────────────────────── */

#define PAGE_PAD            12          /* var(--gap-md) */
#define COL_GAP             12
#define RIGHT_W             380
#define LEFT_W              (UI_SCREEN_W - 2 * PAGE_PAD - COL_GAP - RIGHT_W)
#define CONTENT_INNER_H     (UI_CONTENT_H - 2 * PAGE_PAD)

#define PHASE_COUNT         4
#define PHASE_BAR_H         36
#define INFO_STRIP_H        64
#define HEADER_H            32

/* ─── widgets context ─────────────────────────────────────────────── */

typedef struct {
    /* Левая карточка */
    lv_obj_t *phase_box[PHASE_COUNT];          /* container каждой фазы */
    lv_obj_t *phase_num[PHASE_COUNT];          /* "1".."4" label */
    lv_obj_t *phase_name[PHASE_COUNT];         /* "Нагрев" */
    lv_obj_t *phase_dur[PHASE_COUNT];          /* "30 мин" — показ только active */

    lv_obj_t *info_elapsed;       /* "12:34" */
    lv_obj_t *info_remaining;     /* "17:26" */
    lv_obj_t *info_temp_cur;      /* "32.8°" */
    lv_obj_t *info_temp_tgt;      /* "35.0°" */

    /* Правая колонка — Состояние оборудования */
    lv_obj_t *eq_pump_pre_badge;
    lv_obj_t *eq_heater_badge;
    lv_obj_t *eq_pump_st1_badge;
    lv_obj_t *eq_pump_st2_badge;
    lv_obj_t *eq_doser_badge;

    /* Действия */
    lv_obj_t *btn_heat;
    lv_obj_t *btn_supply;
    lv_obj_t *btn_drain;
    lv_obj_t *btn_cancel;
} washing_widgets_t;

/* ─── helpers ─────────────────────────────────────────────────────── */

static void widgets_free_cb(lv_event_t *e)
{
    void *p = lv_obj_get_user_data(lv_event_get_target(e));
    if (p) lv_free(p);
}

/** Маппинг 7 sub-state'ов → 4 фазы.
 *  Возвращает:
 *    phase  — текущая активная фаза (0..3) или -1 (промывка не идёт)
 *    done   — количество завершённых фаз (0..3) */
static void map_wash_state(wash_sub_state_t ws, plant_state_t st,
                            int *phase, int *done)
{
    *phase = -1;
    *done  = 0;
    if (st != PLANT_STATE_WASHING) return;

    switch (ws) {
    case WASH_SUB_WAIT_HEAT:   *phase = 0; *done = 0; break;
    case WASH_SUB_HEATING:     *phase = 1; *done = 1; break;
    case WASH_SUB_WAIT_SUPPLY: *phase = 2; *done = 2; break;
    case WASH_SUB_SUPPLY:      *phase = 2; *done = 2; break;
    case WASH_SUB_WAIT_DRAIN:  *phase = 3; *done = 3; break;
    case WASH_SUB_DRAIN:       *phase = 3; *done = 3; break;
    case WASH_SUB_DONE:        *phase = -1; *done = 4; break;
    case WASH_SUB_NONE:
    default:                   *phase = -1; *done = 0; break;
    }
}

/* ─── phase track ─────────────────────────────────────────────────── */

static const char *PHASE_NAMES[PHASE_COUNT] = {
    "Ожидание", "Нагрев", "Подача", "Слив"
};
static const char *PHASE_DURATIONS[PHASE_COUNT] = {
    "", "30 мин", "20 мин", ""
};

static void apply_phase_style(washing_widgets_t *w, int idx, int phase, int done)
{
    bool is_done   = (idx < done);
    bool is_active = (idx == phase);

    lv_color_t bg, border, text;

    if (is_done) {
        bg     = ui_token_accent_mute();
        border = ui_token_success();
        text   = ui_token_success();
    } else if (is_active) {
        bg     = ui_token_accent_mute();
        border = ui_token_accent();
        text   = ui_token_accent();
    } else {
        bg     = ui_token_bg_mute();
        border = ui_token_bg_mute();
        text   = ui_token_text_secondary();
    }

    lv_obj_set_style_bg_color(w->phase_box[idx], bg, 0);
    lv_obj_set_style_border_color(w->phase_box[idx], border, 0);
    lv_obj_set_style_text_color(w->phase_num[idx], text, 0);
    lv_obj_set_style_text_color(w->phase_name[idx], text, 0);
    if (w->phase_dur[idx]) {
        lv_obj_set_style_text_color(w->phase_dur[idx], text, 0);
    }
}

static void create_phase_track(lv_obj_t *parent, washing_widgets_t *w,
                                int x, int y, int w_px)
{
    int gap = 4;
    int step_w = (w_px - gap * (PHASE_COUNT - 1)) / PHASE_COUNT;

    for (int i = 0; i < PHASE_COUNT; i++) {
        lv_obj_t *box = lv_obj_create(parent);
        lv_obj_set_pos(box, x + i * (step_w + gap), y);
        lv_obj_set_size(box, step_w, PHASE_BAR_H);
        lv_obj_set_style_radius(box, UI_RADIUS_MD, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(box, 0, 0);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(box, 6, 0);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        w->phase_box[i] = box;

        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        lv_obj_t *n = lv_label_create(box);
        lv_label_set_text(n, num);
        lv_obj_set_style_text_font(n, UI_FONT_MD, 0);
        w->phase_num[i] = n;

        lv_obj_t *name = lv_label_create(box);
        lv_label_set_text(name, PHASE_NAMES[i]);
        lv_obj_set_style_text_font(name, UI_FONT_SM, 0);
        w->phase_name[i] = name;

        if (PHASE_DURATIONS[i][0]) {
            lv_obj_t *dur = lv_label_create(box);
            lv_label_set_text(dur, PHASE_DURATIONS[i]);
            lv_obj_set_style_text_font(dur, UI_FONT_XS, 0);
            w->phase_dur[i] = dur;
        } else {
            w->phase_dur[i] = NULL;
        }

        apply_phase_style(w, i, -1, 0);
    }
}

/* ─── info strip ──────────────────────────────────────────────────── */

static lv_obj_t *create_info_cell(lv_obj_t *parent, const char *label,
                                   lv_color_t value_color, int x, int y, int w_px,
                                   lv_obj_t **out_value)
{
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_set_pos(cell, x, y);
    lv_obj_set_size(cell, w_px, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cell, 0, 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cell, 2, 0);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(cell);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_color(l, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(l, UI_FONT_XS, 0);

    lv_obj_t *v = lv_label_create(cell);
    lv_label_set_text(v, "—");
    lv_obj_set_style_text_color(v, value_color, 0);
    lv_obj_set_style_text_font(v, UI_FONT_XL, 0);
    if (out_value) *out_value = v;

    return cell;
}

static void create_info_strip(lv_obj_t *parent, washing_widgets_t *w,
                               int x, int y, int w_px)
{
    /* Контейнер info-strip — фон bg-mute, скруглённый. */
    lv_obj_t *strip = lv_obj_create(parent);
    lv_obj_set_pos(strip, x, y);
    lv_obj_set_size(strip, w_px, INFO_STRIP_H);
    lv_obj_set_style_bg_color(strip, ui_token_bg_mute(), 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_radius(strip, UI_RADIUS_MD, 0);
    lv_obj_set_style_pad_hor(strip, UI_GAP_LG, 0);
    lv_obj_set_style_pad_ver(strip, 4, 0);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    int cell_w = (w_px - 2 * UI_GAP_LG - 24) / 4;   /* 24 — место для divider'а */

    create_info_cell(strip, "Прошло", ui_token_text_primary(),
                      UI_GAP_LG, 0, cell_w, &w->info_elapsed);
    create_info_cell(strip, "Осталось", ui_token_warning(),
                      UI_GAP_LG + cell_w, 0, cell_w, &w->info_remaining);

    /* Divider — узкая вертикальная полоса 1×36 между группами. */
    lv_obj_t *div = lv_obj_create(strip);
    lv_obj_set_size(div, 1, 36);
    lv_obj_set_pos(div, UI_GAP_LG + 2 * cell_w + 12, (INFO_STRIP_H - 36) / 2);
    lv_obj_set_style_bg_color(div, ui_token_border(), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_radius(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);

    create_info_cell(strip, "Текущая", ui_token_warning(),
                      UI_GAP_LG + 2 * cell_w + 24, 0, cell_w, &w->info_temp_cur);
    create_info_cell(strip, "Целевая", ui_token_text_secondary(),
                      UI_GAP_LG + 3 * cell_w + 24, 0, cell_w, &w->info_temp_tgt);
}

/* ─── wash mnemo placeholder ─────────────────────────────────────── */

static void create_wash_mnemo_placeholder(lv_obj_t *parent, int x, int y, int w_px, int h)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w_px, h);
    lv_obj_set_style_bg_color(p, ui_token_bg_mute(), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(p, ui_token_border_strong(), 0);
    lv_obj_set_style_border_width(p, 1, 0);
    /* dashed border эмулируется через opacity. */
    lv_obj_set_style_radius(p, UI_RADIUS_MD, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(p);
    lv_label_set_text(lbl, "Мнемосхема промывки\n(в разработке)");
    lv_obj_set_style_text_color(lbl, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_MD, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
}

/* ─── right column: equipment status panel ───────────────────────── */

static lv_obj_t *create_status_row(lv_obj_t *parent, const char *label,
                                    bool initial_on)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_color(l, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(l, UI_FONT_SM, 0);

    lv_obj_t *badge = lv_obj_create(row);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(badge, initial_on ? ui_token_accent() : ui_token_text_muted(), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_hor(badge, UI_GAP_SM, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bl = lv_label_create(badge);
    lv_label_set_text(bl, initial_on ? "ON" : "OFF");
    lv_obj_set_style_text_color(bl, ui_token_text_inverse(), 0);
    lv_obj_set_style_text_font(bl, UI_FONT_XS, 0);

    return badge;
}

static void update_status_badge(lv_obj_t *badge, bool on)
{
    if (!badge) return;
    lv_obj_set_style_bg_color(badge,
        on ? ui_token_accent() : ui_token_text_muted(), 0);
    lv_obj_t *bl = lv_obj_get_child(badge, 0);
    if (bl) lv_label_set_text(bl, on ? "ON" : "OFF");
}

/* ─── right column: action button ────────────────────────────────── */

static lv_obj_t *create_action_btn(lv_obj_t *parent, const char *text,
                                    lv_color_t bg, bool disabled,
                                    lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, LV_PCT(100), 40);
    lv_obj_set_style_radius(btn, UI_RADIUS_MD, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    if (disabled) {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(btn, ui_token_bg_mute(), LV_STATE_DISABLED);
    }
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, ui_token_text_inverse(), 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_SM, 0);
    lv_obj_center(lbl);

    return btn;
}

/* ─── create screen ──────────────────────────────────────────────── */

lv_obj_t *scr_washing_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, ui_token_bg_base(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    washing_widgets_t *w = lv_malloc_zeroed(sizeof(*w));
    if (!w) return root;
    lv_obj_set_user_data(root, w);
    lv_obj_add_event_cb(root, widgets_free_cb, LV_EVENT_DELETE, NULL);

    /* ─── Left card ──────────────────────────────────────────────── */
    lv_obj_t *left = lv_obj_create(root);
    lv_obj_set_pos(left, PAGE_PAD, PAGE_PAD);
    lv_obj_set_size(left, LEFT_W, CONTENT_INNER_H);
    lv_obj_set_style_bg_color(left, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(left, ui_token_border(), 0);
    lv_obj_set_style_border_width(left, 1, 0);
    lv_obj_set_style_radius(left, UI_RADIUS_LG, 0);
    lv_obj_set_style_pad_all(left, UI_GAP_MD, 0);
    lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    /* Header: "Режим промывки" + badge "Мойка" */
    lv_obj_t *title = lv_label_create(left);
    lv_label_set_text(title, "Режим промывки");
    lv_obj_set_style_text_color(title, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(title, UI_FONT_LG, 0);
    lv_obj_set_pos(title, 0, 0);

    lv_obj_t *mode_badge = lv_obj_create(left);
    lv_obj_set_size(mode_badge, 80, 24);
    lv_obj_align(mode_badge, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(mode_badge, ui_token_warning(), 0);
    lv_obj_set_style_bg_opa(mode_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mode_badge, 12, 0);
    lv_obj_set_style_border_width(mode_badge, 0, 0);
    lv_obj_set_style_pad_all(mode_badge, 0, 0);
    lv_obj_remove_flag(mode_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *mb_l = lv_label_create(mode_badge);
    lv_label_set_text(mb_l, "Мойка");
    lv_obj_set_style_text_color(mb_l, ui_token_text_inverse(), 0);
    lv_obj_set_style_text_font(mb_l, UI_FONT_SM, 0);
    lv_obj_center(mb_l);

    int left_inner_w = LEFT_W - 2 * UI_GAP_MD;
    int y = HEADER_H + UI_GAP_MD;

    /* Phase track. */
    create_phase_track(left, w, 0, y, left_inner_w);
    y += PHASE_BAR_H + UI_GAP_MD;

    /* Info strip. */
    create_info_strip(left, w, 0, y, left_inner_w);
    y += INFO_STRIP_H + UI_GAP_MD;

    /* Wash mnemo placeholder — fills remaining. */
    int mnemo_h = CONTENT_INNER_H - 2 * UI_GAP_MD - y;
    if (mnemo_h > 100) {
        create_wash_mnemo_placeholder(left, 0, y, left_inner_w, mnemo_h);
    }

    /* ─── Right column: 3 stacked cards ──────────────────────────── */
    int right_x = PAGE_PAD + LEFT_W + COL_GAP;
    int right_y_cursor = PAGE_PAD;
    int card_pad = UI_GAP_MD;

    /* Card 1: Состояние оборудования. */
    int card1_h = 200;
    lv_obj_t *card1 = lv_obj_create(root);
    lv_obj_set_pos(card1, right_x, right_y_cursor);
    lv_obj_set_size(card1, RIGHT_W, card1_h);
    lv_obj_set_style_bg_color(card1, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(card1, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card1, ui_token_border(), 0);
    lv_obj_set_style_border_width(card1, 1, 0);
    lv_obj_set_style_radius(card1, UI_RADIUS_LG, 0);
    lv_obj_set_style_pad_all(card1, card_pad, 0);
    lv_obj_set_flex_flow(card1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card1, UI_GAP_XS, 0);
    lv_obj_remove_flag(card1, LV_OBJ_FLAG_SCROLLABLE);
    right_y_cursor += card1_h + UI_GAP_MD;

    lv_obj_t *t1 = lv_label_create(card1);
    lv_label_set_text(t1, "СОСТОЯНИЕ ОБОРУДОВАНИЯ");
    lv_obj_set_style_text_color(t1, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(t1, UI_FONT_XS, 0);
    lv_obj_set_style_pad_bottom(t1, UI_GAP_XS, 0);

    w->eq_pump_pre_badge = create_status_row(card1, "Преднагнетания", false);
    w->eq_heater_badge   = create_status_row(card1, "Нагреватель (RO4)", false);
    w->eq_pump_st1_badge = create_status_row(card1, "Насос 1-й ст.", false);
    w->eq_pump_st2_badge = create_status_row(card1, "Насос 2-й ст.", false);
    w->eq_doser_badge    = create_status_row(card1, "Дозатор", false);

    /* Card 2: Действия. */
    int card2_h = 220;
    lv_obj_t *card2 = lv_obj_create(root);
    lv_obj_set_pos(card2, right_x, right_y_cursor);
    lv_obj_set_size(card2, RIGHT_W, card2_h);
    lv_obj_set_style_bg_color(card2, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(card2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card2, ui_token_border(), 0);
    lv_obj_set_style_border_width(card2, 1, 0);
    lv_obj_set_style_radius(card2, UI_RADIUS_LG, 0);
    lv_obj_set_style_pad_all(card2, card_pad, 0);
    lv_obj_set_flex_flow(card2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card2, UI_GAP_SM, 0);
    lv_obj_remove_flag(card2, LV_OBJ_FLAG_SCROLLABLE);
    right_y_cursor += card2_h + UI_GAP_MD;

    lv_obj_t *t2 = lv_label_create(card2);
    lv_label_set_text(t2, "ДЕЙСТВИЯ");
    lv_obj_set_style_text_color(t2, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(t2, UI_FONT_XS, 0);
    lv_obj_set_style_pad_bottom(t2, UI_GAP_XS, 0);

    w->btn_heat   = create_action_btn(card2, "Начать нагрев",
                                       ui_token_accent(), true,
                                       ui_evt_start_washing);
    w->btn_supply = create_action_btn(card2, "Подтвердить подачу",
                                       ui_token_accent(), true,
                                       ui_evt_confirm_wash);
    w->btn_drain  = create_action_btn(card2, "Подтвердить слив",
                                       ui_token_bg_elev(), /* placeholder color */
                                       true,
                                       ui_evt_confirm_wash);
    /* (TODO: distinct events for confirm_supply / confirm_drain — пока общий) */
    lv_obj_set_style_bg_color(w->btn_drain, ui_token_text_muted(), 0);

    w->btn_cancel = create_action_btn(card2, "Отменить мойку",
                                       ui_token_danger(), false,
                                       ui_evt_stop);

    /* Card 3: Подсказка. */
    int card3_h = CONTENT_INNER_H - (right_y_cursor - PAGE_PAD);
    if (card3_h > 60) {
        lv_obj_t *card3 = lv_obj_create(root);
        lv_obj_set_pos(card3, right_x, right_y_cursor);
        lv_obj_set_size(card3, RIGHT_W, card3_h);
        lv_obj_set_style_bg_color(card3, ui_token_bg_card(), 0);
        lv_obj_set_style_bg_opa(card3, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(card3, ui_token_border(), 0);
        lv_obj_set_style_border_width(card3, 1, 0);
        lv_obj_set_style_radius(card3, UI_RADIUS_LG, 0);
        lv_obj_set_style_pad_all(card3, card_pad, 0);
        lv_obj_set_flex_flow(card3, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card3, UI_GAP_XS, 0);
        lv_obj_remove_flag(card3, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *t3 = lv_label_create(card3);
        lv_label_set_text(t3, "ПОДСКАЗКА");
        lv_obj_set_style_text_color(t3, ui_token_text_muted(), 0);
        lv_obj_set_style_text_font(t3, UI_FONT_XS, 0);

        lv_obj_t *hint = lv_label_create(card3);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(hint, LV_PCT(100));
        lv_label_set_text(hint,
            "Когда нагрев достигнет уставки 35°C — переключите вручную "
            "клапаны на циркуляцию и нажмите «Подтвердить подачу».");
        lv_obj_set_style_text_color(hint, ui_token_text_secondary(), 0);
        lv_obj_set_style_text_font(hint, UI_FONT_SM, 0);
    }

    return root;
}

/* ─── update ─────────────────────────────────────────────────────── */

void scr_washing_update(lv_obj_t *container, const plant_data_t *data, uint32_t dirty)
{
    (void)dirty;
    if (!container || !data) return;
    washing_widgets_t *w = lv_obj_get_user_data(container);
    if (!w) return;

    /* Phase track. */
    int active_phase = -1, done = 0;
    map_wash_state(data->wash_sub, data->state, &active_phase, &done);
    for (int i = 0; i < PHASE_COUNT; i++) {
        apply_phase_style(w, i, active_phase, done);
    }

    /* Info strip — температура. */
    char buf[16];
    if (w->info_temp_cur) {
        float t = data->temperature.value;
        if (isnan(t)) snprintf(buf, sizeof(buf), "—");
        else          snprintf(buf, sizeof(buf), "%.1f°", t);
        lv_label_set_text(w->info_temp_cur, buf);
    }
    if (w->info_temp_tgt) {
        snprintf(buf, sizeof(buf), "%.1f°", data->set_washing.target_temp_C);
        lv_label_set_text(w->info_temp_tgt, buf);
    }

    /* Time elapsed/remaining — TODO: пока mock значения, реальные придут
     * по MQTT topic'у washing/elapsed_s в фазе 6. */
    if (w->info_elapsed) {
        lv_label_set_text(w->info_elapsed, "--:--");
    }
    if (w->info_remaining) {
        lv_label_set_text(w->info_remaining, "--:--");
    }

    /* Equipment status — по DI/DO битам. */
    update_status_badge(w->eq_pump_pre_badge, (data->di & DI_PUMP1_RUN) != 0);
    update_status_badge(w->eq_pump_st1_badge, (data->di & DI_PUMP2_RUN) != 0);
    update_status_badge(w->eq_pump_st2_badge, (data->di & DI_PUMP3_RUN) != 0);
    update_status_badge(w->eq_doser_badge,    data->doser.enabled);
    /* Нагреватель — по DO биту (TODO: уточнить какой бит). */
    update_status_badge(w->eq_heater_badge, (data->do_bits & 0x08) != 0);

    /* Кнопки — enable/disable по wash_sub. */
    if (w->btn_heat) {
        bool en = (data->wash_sub == WASH_SUB_WAIT_HEAT) ||
                  (data->state != PLANT_STATE_WASHING && data->state != PLANT_STATE_FAULT);
        if (en) lv_obj_remove_state(w->btn_heat, LV_STATE_DISABLED);
        else    lv_obj_add_state(w->btn_heat, LV_STATE_DISABLED);
    }
    if (w->btn_supply) {
        bool en = (data->wash_sub == WASH_SUB_WAIT_SUPPLY);
        if (en) lv_obj_remove_state(w->btn_supply, LV_STATE_DISABLED);
        else    lv_obj_add_state(w->btn_supply, LV_STATE_DISABLED);
    }
    if (w->btn_drain) {
        bool en = (data->wash_sub == WASH_SUB_WAIT_DRAIN);
        if (en) lv_obj_remove_state(w->btn_drain, LV_STATE_DISABLED);
        else    lv_obj_add_state(w->btn_drain, LV_STATE_DISABLED);
    }
}
