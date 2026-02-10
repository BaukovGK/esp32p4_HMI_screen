#pragma once

#include "lvgl.h"

typedef enum {
    SCREEN_MNEMONIC = 0,
    SCREEN_PARAMETERS,
    SCREEN_WASHING,
    SCREEN_ALARMS,
    SCREEN_SETTINGS,
    SCREEN_DIAGNOSTICS,
    SCREEN_COUNT,
} screen_id_t;

void ui_init(lv_display_t *disp);
void ui_switch_screen(screen_id_t id);
