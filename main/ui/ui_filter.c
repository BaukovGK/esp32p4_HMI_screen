/**
 * @file ui_filter.c
 * @brief Реализация виджета фильтра — ромб 40×40 с буквой "Ф".
 */
#include "ui_filter.h"
#include "ui_tokens.h"
#include "ui_fonts.h"

/* Геометрия ромба. Сторона квадрата √2 × сторона ромба для совпадения
 * диагональных размеров. Прототип: ромб 40×40 (от -20 до +20 от центра).
 *   - rect_side = 40 / √2 ≈ 28
 *   - rect повёрнут на 45° → диагональ 40
 * Размещаем rect центром в (cx, cy), затем устанавливаем transform_rotation.
 *
 * Для буквы "Ф" размещаем поверх центра без поворота.
 */
#define DIAMOND_DIAGONAL  40
#define RECT_SIDE         28      /* ≈ 40 / √2 */

lv_obj_t *ui_filter_create(lv_obj_t *parent, int cx, int cy, const char *subtitle)
{
    if (!parent) return NULL;

    /* root container — bbox ромба (40×40) + место под subtitle (~16 px). */
    int root_w = DIAMOND_DIAGONAL + 8;
    int root_h = DIAMOND_DIAGONAL + 24;
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, root_w, root_h);
    /* Размещаем root так, чтобы геометрический центр ромба попал в (cx,cy). */
    lv_obj_set_pos(root, cx - root_w / 2, cy - DIAMOND_DIAGONAL / 2);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* Ромб: повёрнутый на 45° rect. Pivot в его центре. */
    lv_obj_t *diamond = lv_obj_create(root);
    lv_obj_set_size(diamond, RECT_SIDE, RECT_SIDE);
    lv_obj_align(diamond, LV_ALIGN_TOP_MID, 0,
                 (DIAMOND_DIAGONAL - RECT_SIDE) / 2);
    lv_obj_set_style_radius(diamond, 2, 0);
    lv_obj_set_style_bg_color(diamond, ui_token_bg_mute(), 0);
    lv_obj_set_style_bg_opa(diamond, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(diamond, ui_token_border_strong(), 0);
    lv_obj_set_style_border_width(diamond, 2, 0);
    lv_obj_set_style_pad_all(diamond, 0, 0);
    lv_obj_set_style_transform_pivot_x(diamond, RECT_SIDE / 2, 0);
    lv_obj_set_style_transform_pivot_y(diamond, RECT_SIDE / 2, 0);
    lv_obj_set_style_transform_rotation(diamond, 450, 0);   /* 45.0° */
    lv_obj_remove_flag(diamond, LV_OBJ_FLAG_SCROLLABLE);

    /* "Ф" letter — поверх ромба, центрирована, без поворота.
     * Создаётся как sibling diamond'а в root, чтобы не наследовать
     * transform_rotation родителя. */
    lv_obj_t *letter = lv_label_create(root);
    lv_label_set_text(letter, "\xD0\xA4");   /* 'Ф' UTF-8 (D0 A4) */
    lv_obj_set_style_text_color(letter, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(letter, UI_FONT_SM, 0);  /* 14px ≈ proto 14px */
    /* Центр ромба: TOP_MID с offset по y, центр по x. */
    lv_obj_align(letter, LV_ALIGN_TOP_MID, 0, DIAMOND_DIAGONAL / 2 - 8);

    /* Подпись "Фильтр" под ромбом. */
    if (subtitle) {
        lv_obj_t *sub = lv_label_create(root);
        lv_label_set_text(sub, subtitle);
        lv_obj_set_style_text_color(sub, ui_token_text_secondary(), 0);
        lv_obj_set_style_text_font(sub, UI_FONT_XS, 0);
        lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, DIAMOND_DIAGONAL + 2);
    }

    return root;
}
