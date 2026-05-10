/**
 * @file ui_membrane.c
 * @brief Реализация виджета мембраны — rect с label + decorative divider.
 */
#include "ui_membrane.h"
#include "ui_tokens.h"
#include "ui_fonts.h"

lv_obj_t *ui_membrane_create(lv_obj_t *parent, const ui_membrane_config_t *cfg)
{
    if (!parent || !cfg) return NULL;

    lv_obj_t *shell = lv_obj_create(parent);
    lv_obj_set_size(shell, cfg->w, cfg->h);
    lv_obj_set_pos(shell, cfg->x, cfg->y);
    lv_obj_set_style_radius(shell, UI_RADIUS_MD, 0);
    lv_obj_set_style_bg_color(shell, ui_token_accent_mute(), 0);
    lv_obj_set_style_bg_opa(shell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(shell, ui_token_accent(), 0);
    lv_obj_set_style_border_width(shell, 2, 0);
    lv_obj_set_style_pad_all(shell, 0, 0);
    lv_obj_remove_flag(shell, LV_OBJ_FLAG_SCROLLABLE);

    if (cfg->label) {
        lv_obj_t *lbl = lv_label_create(shell);
        lv_label_set_text(lbl, cfg->label);
        lv_obj_set_style_text_color(lbl, ui_token_text_primary(), 0);
        lv_obj_set_style_text_font(lbl, UI_FONT_SM, 0);
        lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, cfg->h / 3 - 4);
    }

    /* Decorative divider — горизонтальная dashed-line ниже центра.
     * Имитирует разделение элементов 4040 в кожухе. Высота 1px,
     * accent цвет с opacity 50%. */
    lv_obj_t *divider = lv_obj_create(shell);
    lv_obj_set_size(divider, cfg->w - 16, 1);
    lv_obj_set_pos(divider, 8, (cfg->h * 2) / 3);
    lv_obj_set_style_bg_color(divider, ui_token_accent(), 0);
    lv_obj_set_style_bg_opa(divider, (LV_OPA_COVER * 50) / 100, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_radius(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    return shell;
}
