#pragma once

#include "lvgl.h"

/* Industrial Dark Color Palette */
#define COLOR_BG_DARK       lv_color_hex(0x1A1A2E)
#define COLOR_BG_PANEL      lv_color_hex(0x16213E)
#define COLOR_BG_WIDGET     lv_color_hex(0x0F3460)
#define COLOR_TEXT_PRIMARY   lv_color_hex(0xE8E8E8)
#define COLOR_TEXT_SECONDARY lv_color_hex(0x8899AA)
#define COLOR_TEXT_VALUE     lv_color_hex(0x00FF88)
#define COLOR_ACCENT         lv_color_hex(0x00B4D8)
#define COLOR_BTN_PRIMARY    lv_color_hex(0x0077B6)
#define COLOR_BTN_DANGER     lv_color_hex(0xE63946)
#define COLOR_BTN_SUCCESS    lv_color_hex(0x2D6A4F)

/* State indicator colors */
#define COLOR_STATE_IDLE     lv_color_hex(0x6C757D)
#define COLOR_STATE_AUTO     lv_color_hex(0x28A745)
#define COLOR_STATE_WASHING  lv_color_hex(0x007BFF)
#define COLOR_STATE_MANUAL   lv_color_hex(0xFFC107)
#define COLOR_STATE_FAULT    lv_color_hex(0xDC3545)

/* Alarm bar colors */
#define COLOR_ALARM_CRITICAL lv_color_hex(0xFF0000)
#define COLOR_ALARM_ALARM    lv_color_hex(0xFF8C00)
#define COLOR_ALARM_WARNING  lv_color_hex(0xFFD700)
#define COLOR_ALARM_INFO     lv_color_hex(0x6C757D)

/* Pump indicator colors */
#define COLOR_PUMP_OFF       lv_color_hex(0x4A4A4A)
#define COLOR_PUMP_STARTING  lv_color_hex(0xFFD700)
#define COLOR_PUMP_RUNNING   lv_color_hex(0x00FF88)
#define COLOR_PUMP_FAULT     lv_color_hex(0xFF0000)

void ui_theme_init(void);

lv_color_t ui_state_color(int state);
lv_color_t ui_alarm_cat_color(int cat);
