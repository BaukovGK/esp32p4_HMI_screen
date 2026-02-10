#pragma once

#include "lvgl.h"

/* Button event callbacks for mode control */
void ui_evt_start_auto(lv_event_t *e);
void ui_evt_stop(lv_event_t *e);
void ui_evt_set_manual(lv_event_t *e);
void ui_evt_start_washing(lv_event_t *e);
void ui_evt_confirm_wash(lv_event_t *e);
void ui_evt_reset_fault(lv_event_t *e);

/* Manual mode pump toggle */
void ui_evt_pump_toggle(lv_event_t *e);

/* Doser enable/disable */
void ui_evt_doser_toggle(lv_event_t *e);

/* Heater on/off (manual mode) */
void ui_evt_heater_toggle(lv_event_t *e);
