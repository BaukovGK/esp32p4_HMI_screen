#pragma once

#include "lvgl.h"
#include "plant_data.h"

/* Alarm bar (40px top strip) */
lv_obj_t *ui_alarm_bar_create(lv_obj_t *parent);
void      ui_alarm_bar_update(lv_obj_t *bar, const plant_data_t *data);

/* Navigation bar (60px bottom strip) */
typedef void (*nav_cb_t)(int screen_id);
lv_obj_t *ui_nav_bar_create(lv_obj_t *parent, nav_cb_t on_select);
void      ui_nav_bar_set_active(lv_obj_t *bar, int screen_id);

/* Formatting helpers */
void ui_fmt_float(char *buf, size_t len, float val, int decimals, const char *unit);
void ui_fmt_uptime(char *buf, size_t len, int64_t seconds);
