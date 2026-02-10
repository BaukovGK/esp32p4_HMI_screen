#pragma once

#include "lvgl.h"

/**
 * Initialize MIPI DSI display with JD9365 driver and LVGL port.
 * Sets landscape orientation (1280x800) via software rotation.
 * @return LVGL display handle, or NULL on failure
 */
lv_display_t *display_init(void);
