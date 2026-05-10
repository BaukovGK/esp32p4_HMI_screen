/**
 * @file ui_modal.c
 * @brief Generic modal-overlay infrastructure.
 *
 * Overlay структура:
 *   overlay (lv_obj_t, full screen)
 *     ├── backdrop (lv_obj_t, full screen, transp black 60%, clickable)
 *     │       — клик закрывает overlay (через event_cb)
 *     └── card (lv_obj_t, centered, rounded, foreground)
 *             ├── header
 *             │     ├── title_block (title + subtitle column)
 *             │     └── close_btn (×)
 *             ├── body (flex column, gap, scrollable)
 *             │     ↑ возвращается из ui_modal_open()
 *             └── footer
 *                   └── close_btn_text [Закрыть]
 *
 * body хранится в user_data overlay'а для quick access из helpers.
 *
 * Закрытие:
 *   - клик по backdrop → ui_modal_close (но НЕ при клике по card)
 *   - клик по × или [Закрыть] → ui_modal_close
 */
#include "ui_modal.h"
#include "ui_tokens.h"
#include "ui_fonts.h"
#include "lang.h"
#include <stdio.h>

/* Размеры модала. */
#define MODAL_W            560
#define MODAL_H_MAX        640
#define HEADER_H           64
#define FOOTER_H           56

/* ─── helpers ─────────────────────────────────────────────────────── */

static void modal_close_event_cb(lv_event_t *e)
{
    /* user_data — указатель на overlay. */
    lv_obj_t *overlay = lv_event_get_user_data(e);
    if (overlay) {
        lv_obj_delete_async(overlay);
    }
}

static void backdrop_click_cb(lv_event_t *e)
{
    /* Закрываем только если клик пришёл ИМЕННО на backdrop (не на card). */
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *current = lv_event_get_current_target(e);
    if (target == current) {
        lv_obj_t *overlay = lv_event_get_user_data(e);
        if (overlay) lv_obj_delete_async(overlay);
    }
}

/* ─── публичный API ───────────────────────────────────────────────── */

lv_obj_t *ui_modal_open(const char *title, const char *subtitle)
{
    lv_obj_t *scr = lv_screen_active();

    /* 1. Overlay — корневой объект на весь экран, semi-transparent. */
    lv_obj_t *overlay = lv_obj_create(scr);
    lv_obj_set_size(overlay, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, ((lv_color_t)LV_COLOR_MAKE(0, 0, 0)), 0);
    lv_obj_set_style_bg_opa(overlay, (LV_OPA_COVER * 60) / 100, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    /* Backdrop click → close (event_cb на overlay, проверка target == this). */
    lv_obj_add_event_cb(overlay, backdrop_click_cb, LV_EVENT_CLICKED, overlay);

    /* 2. Card — центрированная карточка. */
    lv_obj_t *card = lv_obj_create(overlay);
    lv_obj_set_size(card, MODAL_W, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(card, MODAL_H_MAX, 0);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, UI_RADIUS_LG, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, ui_token_border(), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 24, 0);
    lv_obj_set_style_shadow_color(card, ((lv_color_t)LV_COLOR_MAKE(0, 0, 0)), 0);
    lv_obj_set_style_shadow_opa(card, (LV_OPA_COVER * 30) / 100, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 3. Header — title + subtitle + close button. */
    lv_obj_t *header = lv_obj_create(card);
    lv_obj_set_size(header, LV_PCT(100), HEADER_H);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, ui_token_border(), 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_hor(header, UI_GAP_LG, 0);
    lv_obj_set_style_pad_ver(header, UI_GAP_MD, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* Title-block (column для title + subtitle). */
    lv_obj_t *title_block = lv_obj_create(header);
    lv_obj_set_size(title_block, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(title_block, 1);
    lv_obj_set_style_bg_opa(title_block, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_block, 0, 0);
    lv_obj_set_style_pad_all(title_block, 0, 0);
    lv_obj_set_flex_flow(title_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(title_block, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(title_block, 2, 0);
    lv_obj_remove_flag(title_block, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_lbl = lv_label_create(title_block);
    lv_label_set_text(title_lbl, title ? title : "");
    lv_obj_set_style_text_color(title_lbl, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(title_lbl, UI_FONT_LG, 0);
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title_lbl, LV_PCT(100));

    if (subtitle) {
        lv_obj_t *sub_lbl = lv_label_create(title_block);
        lv_label_set_text(sub_lbl, subtitle);
        lv_obj_set_style_text_color(sub_lbl, ui_token_text_secondary(), 0);
        lv_obj_set_style_text_font(sub_lbl, UI_FONT_SM, 0);
        lv_label_set_long_mode(sub_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(sub_lbl, LV_PCT(100));
    }

    /* × close button. */
    lv_obj_t *close_btn = lv_button_create(header);
    lv_obj_set_size(close_btn, 36, 36);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_pad_all(close_btn, 0, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, modal_close_event_cb, LV_EVENT_CLICKED, overlay);

    lv_obj_t *x_lbl = lv_label_create(close_btn);
    lv_label_set_text(x_lbl, "x");   /* ASCII fallback; × (U+D7) тоже отсутствует в Montserrat */
    lv_obj_set_style_text_color(x_lbl, ui_token_text_secondary(), 0);
    lv_obj_set_style_text_font(x_lbl, UI_FONT_LG, 0);
    lv_obj_center(x_lbl);

    /* 4. Body — flex column со скроллом если контента много. */
    lv_obj_t *body = lv_obj_create(card);
    lv_obj_set_size(body, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(body, MODAL_H_MAX - HEADER_H - FOOTER_H, 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_style_pad_all(body, UI_GAP_LG, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(body, UI_GAP_LG, 0);

    /* 5. Footer — [Закрыть] button. */
    lv_obj_t *footer = lv_obj_create(card);
    lv_obj_set_size(footer, LV_PCT(100), FOOTER_H);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(footer, 1, 0);
    lv_obj_set_style_border_color(footer, ui_token_border(), 0);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, UI_GAP_MD, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *close_text_btn = lv_button_create(footer);
    lv_obj_set_size(close_text_btn, 120, 40);
    lv_obj_set_style_radius(close_text_btn, UI_RADIUS_MD, 0);
    lv_obj_set_style_bg_color(close_text_btn, ui_token_bg_mute(), 0);
    lv_obj_set_style_border_width(close_text_btn, 1, 0);
    lv_obj_set_style_border_color(close_text_btn, ui_token_border(), 0);
    lv_obj_set_style_shadow_width(close_text_btn, 0, 0);
    lv_obj_add_event_cb(close_text_btn, modal_close_event_cb, LV_EVENT_CLICKED, overlay);
    lv_obj_t *close_lbl = lv_label_create(close_text_btn);
    lv_label_set_text(close_lbl, "Закрыть");
    lv_obj_set_style_text_color(close_lbl, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(close_lbl, UI_FONT_MD, 0);
    lv_obj_center(close_lbl);

    /* Сохранить body в user_data overlay'а — для quick access. */
    lv_obj_set_user_data(overlay, body);

    return body;
}

void ui_modal_close(lv_obj_t *body_or_overlay)
{
    if (!body_or_overlay) return;
    /* Найти overlay — пройти вверх по parent'ам. */
    lv_obj_t *o = body_or_overlay;
    while (o && lv_obj_get_parent(o) != lv_screen_active()) {
        o = lv_obj_get_parent(o);
    }
    if (o) lv_obj_delete_async(o);
}

/* ─── секции и rows ──────────────────────────────────────────────── */

lv_obj_t *ui_modal_add_section(lv_obj_t *body, const char *title)
{
    if (!body) return NULL;

    lv_obj_t *section = lv_obj_create(body);
    lv_obj_set_size(section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(section, 0, 0);
    lv_obj_set_style_radius(section, 0, 0);
    lv_obj_set_style_pad_all(section, 0, 0);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(section, UI_GAP_SM, 0);
    lv_obj_remove_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    if (title) {
        lv_obj_t *t = lv_label_create(section);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_color(t, ui_token_text_muted(), 0);
        lv_obj_set_style_text_font(t, UI_FONT_XS, 0);
    }

    return section;
}

void ui_modal_add_prop_row(lv_obj_t *section, const char *key, const char *val)
{
    if (!section) return;

    lv_obj_t *row = lv_obj_create(section);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *k = lv_label_create(row);
    lv_label_set_text(k, key ? key : "");
    lv_obj_set_style_text_color(k, ui_token_text_secondary(), 0);
    lv_obj_set_style_text_font(k, UI_FONT_SM, 0);

    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, val ? val : "—");
    lv_obj_set_style_text_color(v, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(v, UI_FONT_SM, 0);
}

static lv_color_t state_color(ui_modal_state_t s)
{
    switch (s) {
    case UI_MODAL_STATE_OK:      return ui_token_accent();
    case UI_MODAL_STATE_WARN:    return ui_token_warning();
    case UI_MODAL_STATE_DANGER:  return ui_token_danger();
    case UI_MODAL_STATE_OFFLINE:
    default:                     return ui_token_text_muted();
    }
}

void ui_modal_add_current_value(lv_obj_t *section,
                                 const char *value,
                                 const char *unit,
                                 ui_modal_state_t state,
                                 const char *state_label)
{
    if (!section) return;

    lv_obj_t *row = lv_obj_create(section);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, ui_token_bg_mute(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, UI_RADIUS_MD, 0);
    lv_obj_set_style_pad_all(row, UI_GAP_MD, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, UI_GAP_SM, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Value + unit (большое значение). */
    lv_obj_t *val_block = lv_obj_create(row);
    lv_obj_set_size(val_block, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(val_block, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(val_block, 0, 0);
    lv_obj_set_style_pad_all(val_block, 0, 0);
    lv_obj_set_flex_flow(val_block, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(val_block, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(val_block, 4, 0);
    lv_obj_remove_flag(val_block, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *v = lv_label_create(val_block);
    lv_label_set_text(v, value ? value : "—");
    lv_obj_set_style_text_color(v, state_color(state), 0);
    lv_obj_set_style_text_font(v, UI_FONT_XL, 0);

    if (unit) {
        lv_obj_t *u = lv_label_create(val_block);
        lv_label_set_text(u, unit);
        lv_obj_set_style_text_color(u, ui_token_text_secondary(), 0);
        lv_obj_set_style_text_font(u, UI_FONT_MD, 0);
    }

    /* State badge. */
    if (state_label) {
        lv_obj_t *badge = lv_obj_create(row);
        lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(badge, state_color(state), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge, 12, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_pad_hor(badge, UI_GAP_MD, 0);
        lv_obj_set_style_pad_ver(badge, 4, 0);
        lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *bl = lv_label_create(badge);
        lv_label_set_text(bl, state_label);
        lv_obj_set_style_text_color(bl, ui_token_text_inverse(), 0);
        lv_obj_set_style_text_font(bl, UI_FONT_SM, 0);
    }
}

void ui_modal_add_range_bar(lv_obj_t *section, int pct,
                            const char *min_label, const char *max_label)
{
    if (!section) return;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    /* Сама полоса (rect 100% width, 8 px tall). */
    lv_obj_t *bar = lv_obj_create(section);
    lv_obj_set_size(bar, LV_PCT(100), 12);
    lv_obj_set_style_bg_color(bar, ui_token_bg_mute(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 6, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Pointer — узкая вертикальная полоса accent. */
    lv_obj_t *ptr = lv_obj_create(bar);
    lv_obj_set_size(ptr, 4, 18);
    lv_obj_align(ptr, LV_ALIGN_LEFT_MID, 0, 0);
    /* x position = pct% of (parent_w - ptr_w). LV_PCT не комбинируется
     * с явным offset легко — используем lv_obj_set_x с расчётом ниже. */
    lv_obj_set_style_bg_color(ptr, ui_token_accent(), 0);
    lv_obj_set_style_bg_opa(ptr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ptr, 2, 0);
    lv_obj_set_style_border_width(ptr, 0, 0);
    lv_obj_set_style_pad_all(ptr, 0, 0);

    /* Position pointer: по событию layout. Простая аппроксимация —
     * считаем что bar = MODAL_W - 2*GAP_LG = 528, минус ptr_w = 4. */
    int bar_w_approx = 560 - 2 * UI_GAP_LG;
    int x = ((bar_w_approx - 4) * pct) / 100;
    lv_obj_set_x(ptr, x);

    /* Метки min/max снизу. */
    lv_obj_t *labels = lv_obj_create(section);
    lv_obj_set_size(labels, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(labels, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(labels, 0, 0);
    lv_obj_set_style_pad_all(labels, 0, 0);
    lv_obj_set_flex_flow(labels, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(labels, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(labels, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lmin = lv_label_create(labels);
    lv_label_set_text(lmin, min_label ? min_label : "");
    lv_obj_set_style_text_color(lmin, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(lmin, UI_FONT_XS, 0);

    lv_obj_t *lmax = lv_label_create(labels);
    lv_label_set_text(lmax, max_label ? max_label : "");
    lv_obj_set_style_text_color(lmax, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(lmax, UI_FONT_XS, 0);
}
