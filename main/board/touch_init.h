#pragma once

#include "lvgl.h"

/**
 * Initialize GSL3680 capacitive touch controller and register with LVGL.
 * @param disp LVGL display to associate touch input with
 * @return LVGL input device handle, or NULL on failure
 */
lv_indev_t *touch_init(lv_display_t *disp);
