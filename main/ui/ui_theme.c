#include "ui_theme.h"
#include "ui_fonts.h"
#include "plant_data.h"

void ui_theme_init(void)
{
    lv_display_t *disp = lv_display_get_default();
    if (!disp) return;

    lv_theme_t *theme = lv_theme_default_init(disp,
        COLOR_ACCENT, COLOR_BTN_PRIMARY, true, UI_FONT_14);
    lv_display_set_theme(disp, theme);

    /* Set default screen background */
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

lv_color_t ui_state_color(int state)
{
    switch (state) {
    case PLANT_STATE_IDLE:    return COLOR_STATE_IDLE;
    case PLANT_STATE_AUTO:    return COLOR_STATE_AUTO;
    case PLANT_STATE_WASHING: return COLOR_STATE_WASHING;
    case PLANT_STATE_MANUAL:  return COLOR_STATE_MANUAL;
    case PLANT_STATE_FAULT:   return COLOR_STATE_FAULT;
    default:                  return COLOR_STATE_IDLE;
    }
}

lv_color_t ui_alarm_cat_color(int cat)
{
    switch (cat) {
    case ALARM_CAT_CRITICAL: return COLOR_ALARM_CRITICAL;
    case ALARM_CAT_ALARM:    return COLOR_ALARM_ALARM;
    case ALARM_CAT_WARNING:  return COLOR_ALARM_WARNING;
    default:                 return COLOR_ALARM_INFO;
    }
}
