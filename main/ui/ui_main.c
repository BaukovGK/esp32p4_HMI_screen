#include "ui_main.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "plant_data.h"
#include "board.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Screen function declarations */
#include "scr_mnemonic.h"
#include "scr_parameters.h"
#include "scr_washing.h"
#include "scr_alarms.h"
#include "scr_settings.h"
#include "scr_diagnostics.h"

static const char *TAG = "ui_main";

typedef struct {
    lv_obj_t *(*create)(lv_obj_t *parent);
    void      (*update)(lv_obj_t *container, const plant_data_t *data);
} screen_desc_t;

static const screen_desc_t s_screens[SCREEN_COUNT] = {
    [SCREEN_MNEMONIC]    = { scr_mnemonic_create,    scr_mnemonic_update },
    [SCREEN_PARAMETERS]  = { scr_parameters_create,  scr_parameters_update },
    [SCREEN_WASHING]     = { scr_washing_create,     scr_washing_update },
    [SCREEN_ALARMS]      = { scr_alarms_create,      scr_alarms_update },
    [SCREEN_SETTINGS]    = { scr_settings_create,    scr_settings_update },
    [SCREEN_DIAGNOSTICS] = { scr_diagnostics_create, scr_diagnostics_update },
};

static screen_id_t s_current = SCREEN_MNEMONIC;
static lv_obj_t   *s_content  = NULL;
static lv_obj_t   *s_alarm_bar = NULL;
static lv_obj_t   *s_nav_bar  = NULL;
static lv_obj_t   *s_screen_obj = NULL;

static void on_nav_select(int screen_id)
{
    if (screen_id >= 0 && screen_id < SCREEN_COUNT) {
        ui_switch_screen((screen_id_t)screen_id);
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!plant_data_lock(5)) return;

    const plant_data_t *data = plant_data_get();

    /* Always update alarm bar */
    ui_alarm_bar_update(s_alarm_bar, data);

    /* Update current screen */
    if (s_screens[s_current].update && s_screen_obj) {
        s_screens[s_current].update(s_screen_obj, data);
    }

    plant_data_unlock();
}

void ui_init(lv_display_t *disp)
{
    (void)disp;
    ESP_LOGI(TAG, "Initializing UI");

    /* Phase 1: theme + background */
    lvgl_port_lock(0);
    ui_theme_init();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Phase 2: alarm bar */
    lvgl_port_lock(0);
    s_alarm_bar = ui_alarm_bar_create(scr);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Phase 3: content area + nav bar */
    lvgl_port_lock(0);
    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, BOARD_DISP_H_RES, 700);
    lv_obj_set_pos(s_content, 0, 40);
    lv_obj_set_style_bg_color(s_content, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_content, 0, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 0, 0);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    s_nav_bar = ui_nav_bar_create(scr, on_nav_select);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Phase 4: first screen (start with lightest for fast boot) */
    lvgl_port_lock(0);
    s_current = SCREEN_DIAGNOSTICS;
    s_screen_obj = s_screens[SCREEN_DIAGNOSTICS].create(s_content);
    ui_nav_bar_set_active(s_nav_bar, SCREEN_DIAGNOSTICS);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Phase 5: refresh timer */
    lvgl_port_lock(0);
    lv_timer_create(refresh_timer_cb, 250, NULL);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "UI initialized");
}

void ui_switch_screen(screen_id_t id)
{
    if (id >= SCREEN_COUNT || id == s_current) return;

    lvgl_port_lock(0);

    /* Delete old screen content */
    if (s_screen_obj) {
        lv_obj_delete(s_screen_obj);
        s_screen_obj = NULL;
    }

    /* Create new screen */
    s_current = id;
    s_screen_obj = s_screens[id].create(s_content);

    /* Update nav bar highlight */
    ui_nav_bar_set_active(s_nav_bar, id);

    lvgl_port_unlock();

    ESP_LOGI(TAG, "Switched to screen %d", id);
}
