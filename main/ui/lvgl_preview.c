/*
 * lvgl_preview.c -- Entry point for LVGL Live Preview extension
 *
 * Provides lvgl_live_preview_init() and stub implementations
 * for all ESP-IDF dependencies (plant_data, alarm_ring, mqtt, etc.)
 *
 * Press Ctrl+Shift+L in VSCode to launch the preview.
 */

#ifdef LVGL_LIVE_PREVIEW

#include "lvgl.h"
#include "ui_main.h"
#include "ui_theme.h"
#include "ui_tokens.h"
#include "ui_common.h"
#include "ui_statusbar.h"
#include "ui_tabbar.h"
#include "ui_fonts.h"
#include "lang.h"
#include "plant_data.h"
#include "alarm_ring.h"
#include "mqtt_app.h"

/* Screen headers */
#include "scr_mnemonic.h"
#include "scr_parameters.h"
#include "scr_washing.h"
#include "scr_alarms.h"
#include "scr_settings.h"
#include "scr_diagnostics.h"

#include <string.h>
#include <math.h>

/* ================================================================
 *  Stub: plant_data (mock sensor values, no FreeRTOS mutex)
 * ================================================================ */

static plant_data_t s_preview_data;

void plant_data_init(void)
{
    memset(&s_preview_data, 0, sizeof(s_preview_data));

    /* State: running in AUTO mode */
    s_preview_data.state    = PLANT_STATE_AUTO;
    s_preview_data.auto_sub = AUTO_SUB_RUNNING;

    /* Digital I/O: tanks half-full, 3 pumps running */
    s_preview_data.di      = 0x33;  /* source low+high, interm low+high */
    s_preview_data.do_bits = 0x07;  /* pump1 + pump2 + pump3 */

    /* Pressure P1..P4 (bar) */
    s_preview_data.pressure[0] = (analog_channel_t){ 3.21f, false };
    s_preview_data.pressure[1] = (analog_channel_t){ 2.85f, false };
    s_preview_data.pressure[2] = (analog_channel_t){25.50f, false };
    s_preview_data.pressure[3] = (analog_channel_t){ 5.12f, false };

    /* Temperature */
    s_preview_data.temperature = (analog_channel_t){ 24.5f, false };

    /* Flow Q1..Q4 (m3/h) */
    s_preview_data.flow[0] = (flow_channel_t){ 1.50f, 1234.5f, true };
    s_preview_data.flow[1] = (flow_channel_t){ 1.80f,  987.2f, true };
    s_preview_data.flow[2] = (flow_channel_t){ 2.10f, 2345.6f, true };
    s_preview_data.flow[3] = (flow_channel_t){ 0.35f,  456.7f, true };

    /* Conductivity S1..S3 (uS/cm) */
    s_preview_data.conductivity[0] = (cond_channel_t){ 450.0f, 24.5f, true };
    s_preview_data.conductivity[1] = (cond_channel_t){  12.3f, 24.8f, true };
    s_preview_data.conductivity[2] = (cond_channel_t){   8.7f, 25.0f, true };

    /* Calculated telemetry */
    s_preview_data.telemetry = (telemetry_t){
        .filter_dp    = 0.42f,
        .stage1_feed  = 3.30f,
        .recovery2    = 75.0f,
        .recovery_sys = 72.5f,
        .sel1         = 97.3f,
        .sel2         = 98.1f,
    };

    /* Subsystems */
    s_preview_data.doser = (doser_status_t){ DOSER_STATE_RUNNING, true };
    s_preview_data.mqtt_connected = true;

    /* Diagnostics */
    s_preview_data.diagnostics = (diagnostics_t){
        .heap_free      = 180000,
        .heap_min       = 120000,
        .uptime_s       = 86400 + 3661,  /* 1d 1h 1m 1s */
        .stack_modbus   = 2048,
        .stack_io       = 1536,
        .stack_process  = 2560,
        .stack_watchdog = 1024,
        .stack_mqtt     = 3072,
        .modbus_online  = { true, true, false, true },
        .modbus_errors  = { 0, 2, 15, 0 },
    };

    /* Default settings */
    s_preview_data.set_pressure = (settings_pressure_t){
        .p1_max = 5.5f, .p3_max = 35.0f,
        .p4_max = 8.0f, .filter_dp_warn = 1.0f,
    };
    s_preview_data.set_doser = (settings_doser_t){
        .run_time_min = 5, .cycle_time_min = 60,
    };
    s_preview_data.set_washing = (settings_washing_t){
        .target_temp_C = 35.0f, .max_temp_C = 40.0f,
        .t_overshoot_C = 45.0f, .hysteresis_C = 2.0f,
        .heat_timeout_min = 30, .supply_time_min = 20, .drain_time_min = 5,
    };
    s_preview_data.set_timeouts = (settings_timeouts_t){
        .pump_confirm_ms = 3000, .pump_ramp_ms = 15000,
    };
}

bool plant_data_lock(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return true;
}

void plant_data_unlock(void) { }

const plant_data_t *plant_data_get(void)
{
    return &s_preview_data;
}

plant_data_t *plant_data_get_mutable(void)
{
    return &s_preview_data;
}

uint32_t plant_data_get_and_clear_dirty(void) { return ~0u; }

/* Setters (no-op in preview) */
void plant_data_set_state(plant_state_t s, auto_sub_state_t a,
                          wash_sub_state_t w, uint32_t f)
{ (void)s; (void)a; (void)w; (void)f; }

void plant_data_set_io(uint8_t di, uint8_t d)
{ (void)di; (void)d; }

void plant_data_set_pressure(int i, float v, bool f)
{ (void)i; (void)v; (void)f; }

void plant_data_set_temperature(float v, bool f)
{ (void)v; (void)f; }

void plant_data_set_flow(int i, float fl, float vol, bool ok)
{ (void)i; (void)fl; (void)vol; (void)ok; }

void plant_data_set_conductivity(int i, float c, float t, bool ok)
{ (void)i; (void)c; (void)t; (void)ok; }

void plant_data_set_telemetry(const telemetry_t *t)   { (void)t; }
void plant_data_set_interlocks(uint32_t f, bool e, bool w)
{ (void)f; (void)e; (void)w; }

void plant_data_set_doser(doser_state_t s, bool e)
{ (void)s; (void)e; }

void plant_data_set_diagnostics(const diagnostics_t *d) { (void)d; }
void plant_data_set_mqtt_status(bool c) { (void)c; }

void plant_data_set_power_meter(int i, const power_meter_data_t *d)
{ (void)i; (void)d; }

power_meter_data_t plant_data_get_power_lp(void)
{
    power_meter_data_t z = { NAN, NAN, NAN, NAN, NAN, false };
    return z;
}
power_meter_data_t plant_data_get_power_hp(void)
{
    power_meter_data_t z = { NAN, NAN, NAN, NAN, NAN, false };
    return z;
}

/* NVS save stubs (no-op in preview) */
void plant_data_save_settings_pressure(const settings_pressure_t *s)  { (void)s; }
void plant_data_save_settings_doser(const settings_doser_t *s)        { (void)s; }
void plant_data_save_settings_washing(const settings_washing_t *s)    { (void)s; }
void plant_data_save_settings_timeouts(const settings_timeouts_t *s)  { (void)s; }

/* ================================================================
 *  Stub: alarm_ring (static mock alarms)
 * ================================================================ */

static alarm_entry_t s_mock_alarms[] = {
    { 1, 1000, ALARM_CAT_WARNING,  "FILTER_DP_HIGH", 1.2f, true  },
    { 2,  900, ALARM_CAT_ALARM,    "P3_HIGH",       36.5f, true  },
    { 3,  800, ALARM_CAT_INFO,     "MQTT_RECONNECT",  NAN, false },
};

void alarm_ring_init(void) { }
void alarm_ring_push(const alarm_entry_t *e) { (void)e; }

int alarm_ring_get_active(alarm_entry_t *out, int max_count)
{
    int n = 0;
    for (int i = 0; i < 3 && n < max_count; i++) {
        if (s_mock_alarms[i].active) out[n++] = s_mock_alarms[i];
    }
    return n;
}

int alarm_ring_get_history(alarm_entry_t *out, int max_count)
{
    int n = (max_count < 3) ? max_count : 3;
    for (int i = 0; i < n; i++) out[i] = s_mock_alarms[i];
    return n;
}

int alarm_ring_active_count(void) { return 2; }
alarm_category_t alarm_ring_worst_active(void) { return ALARM_CAT_ALARM; }
uint32_t alarm_ring_generation(void) { return 1; }
void alarm_ring_clear(void) { }

/* ================================================================
 *  Stub: MQTT publish functions (no-op in preview)
 * ================================================================ */

esp_err_t mqtt_publish_mode_cmd(const char *cmd) { (void)cmd; return 0; }
esp_err_t mqtt_publish_pump_mask(uint8_t mask)   { (void)mask; return 0; }
esp_err_t mqtt_publish_doser_enable(bool en)     { (void)en; return 0; }
esp_err_t mqtt_publish_heater(bool on)           { (void)on; return 0; }

esp_err_t mqtt_publish_settings_pressure(const settings_pressure_t *s)
{ (void)s; return 0; }

esp_err_t mqtt_publish_settings_doser(const settings_doser_t *s)
{ (void)s; return 0; }

esp_err_t mqtt_publish_settings_washing(const settings_washing_t *s)
{ (void)s; return 0; }

esp_err_t mqtt_publish_settings_timeouts(const settings_timeouts_t *s)
{ (void)s; return 0; }

/* ================================================================
 *  Screen navigation (simplified version of ui_main.c)
 * ================================================================ */

typedef struct {
    lv_obj_t *(*create)(lv_obj_t *parent);
    void      (*update)(lv_obj_t *container, const plant_data_t *data, uint32_t dirty);
} preview_screen_t;

static const preview_screen_t s_screens[] = {
    { scr_mnemonic_create,    scr_mnemonic_update },
    { scr_parameters_create,  scr_parameters_update },
    { scr_washing_create,     scr_washing_update },
    { scr_alarms_create,      scr_alarms_update },
    { scr_settings_create,    scr_settings_update },
    { scr_diagnostics_create, scr_diagnostics_update },
};

static int       s_current_screen = 0;
static lv_obj_t *s_content        = NULL;
static lv_obj_t *s_statusbar      = NULL;
static lv_obj_t *s_tabbar         = NULL;
static lv_obj_t *s_screen_obj     = NULL;

/* Маппинг screen_id_t (6 экранов) → ui_tab_id_t (4 вкладки tabbar).
 * Для экранов вне tabbar (PARAMETERS, ALARMS) возвращаем UI_TAB_COUNT. */
static ui_tab_id_t screen_to_tab(int screen_id)
{
    switch (screen_id) {
    case SCREEN_MNEMONIC:    return UI_TAB_MAIN;
    case SCREEN_WASHING:     return UI_TAB_WASHING;
    case SCREEN_SETTINGS:    return UI_TAB_SETTINGS;
    case SCREEN_DIAGNOSTICS: return UI_TAB_DEBUG;
    default:                 return UI_TAB_COUNT;
    }
}

static int tab_to_screen(ui_tab_id_t tab)
{
    switch (tab) {
    case UI_TAB_MAIN:     return SCREEN_MNEMONIC;
    case UI_TAB_WASHING:  return SCREEN_WASHING;
    case UI_TAB_SETTINGS: return SCREEN_SETTINGS;
    case UI_TAB_DEBUG:    return SCREEN_DIAGNOSTICS;
    default:              return SCREEN_MNEMONIC;
    }
}

static void preview_switch_screen(int id)
{
    if (id < 0 || id >= SCREEN_COUNT || id == s_current_screen) return;

    if (s_screen_obj) {
        lv_obj_delete(s_screen_obj);
        s_screen_obj = NULL;
    }

    s_current_screen = id;
    s_screen_obj = s_screens[id].create(s_content);

    ui_tab_id_t tab = screen_to_tab(id);
    if (tab != UI_TAB_COUNT) {
        ui_tabbar_set_active(s_tabbar, tab);
    }

    /* Immediately fill with mock data */
    if (s_screens[id].update && s_screen_obj) {
        s_screens[id].update(s_screen_obj, &s_preview_data, ~0u);
    }
}

static void on_tab_select(ui_tab_id_t tab)
{
    preview_switch_screen(tab_to_screen(tab));
}

static void on_theme_change(void)
{
    /* TODO: пересоздать UI после переключения темы. Пока no-op —
     * новые цвета подхватятся при следующем lv_obj_invalidate. */
}

static void preview_refresh_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_statusbar_update(s_statusbar, &s_preview_data);
    if (s_screens[s_current_screen].update && s_screen_obj) {
        s_screens[s_current_screen].update(s_screen_obj, &s_preview_data, ~0u);
    }
}

/* ================================================================
 *  Entry point for LVGL Live Preview extension
 * ================================================================ */

void lvgl_live_preview_init(void)
{
    /* Initialize subsystems */
    lang_init(LANG_RU);
    plant_data_init();
    alarm_ring_init();

    /* Theme & background — используем новые токены (light по умолчанию). */
    ui_tokens_set_theme(UI_THEME_LIGHT);
    ui_theme_init();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, ui_token_bg_base(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Statusbar (44px top strip) — новый дизайн прототипа. */
    s_statusbar = ui_statusbar_create(scr, on_theme_change);

    /* Content area (1280 × 692). */
    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(s_content, 0, UI_STATUSBAR_H);
    lv_obj_set_style_bg_color(s_content, ui_token_bg_base(), 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_content, 0, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 0, 0);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    /* Tabbar (64px bottom) — 4 вкладки MVP. */
    s_tabbar = ui_tabbar_create(scr, on_tab_select);

    /* Создание стартового экрана — мнемосхема. */
    s_current_screen = SCREEN_MNEMONIC;
    s_screen_obj = s_screens[SCREEN_MNEMONIC].create(s_content);
    ui_tabbar_set_active(s_tabbar, UI_TAB_MAIN);

    /* Заполнить mock-данными сразу. */
    ui_statusbar_update(s_statusbar, &s_preview_data);
    s_screens[SCREEN_MNEMONIC].update(s_screen_obj, &s_preview_data, ~0u);

    /* Periodic refresh (250 ms, как на реальном HMI). */
    lv_timer_create(preview_refresh_cb, 250, NULL);
}

#endif /* LVGL_LIVE_PREVIEW */
