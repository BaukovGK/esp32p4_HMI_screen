/**
 * @file ui_main.c
 * @brief Главный модуль UI -- инициализация интерфейса и управление экранами.
 *
 * После миграции (фаза 1 LVGL_IMPLEMENTATION_PLAN.md):
 *  - statusbar (44px) на месте старого alarm_bar (40px)
 *  - tabbar (64px) на месте старого nav_bar (60px) — 4 вкладки вместо 6
 *  - размеры теперь из ui_tokens.h: UI_STATUSBAR_H, UI_TABBAR_H, UI_CONTENT_H
 *
 * MVP-навигация: Главная / Промывка / Настройки / Отладка. Параметры и
 * Алармы временно недоступны через tabbar — они должны быть портированы
 * как под-страницы Главной (UI_SPEC §1).
 *
 * Существующие screen_id_t значения сохранены для обратной совместимости
 * (старые экраны продолжают компилироваться и могут быть открыты программно
 * через ui_switch_screen). Маппинг tab → screen — в TAB_TO_SCREEN ниже.
 */
#ifndef LVGL_LIVE_PREVIEW
#include "ui_main.h"
#include "ui_theme.h"
#include "ui_tokens.h"
#include "ui_common.h"
#include "ui_statusbar.h"
#include "ui_tabbar.h"
#include "plant_data.h"
#include "board.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Заголовки экранов */
#include "scr_mnemonic.h"
#include "scr_parameters.h"
#include "scr_washing.h"
#include "scr_alarms.h"
#include "scr_settings.h"
#include "scr_diagnostics.h"

static const char *TAG = "ui_main";

#define STALE_DATA_TIMEOUT_US  (5 * 1000000LL)

typedef struct {
    lv_obj_t *(*create)(lv_obj_t *parent);
    void      (*update)(lv_obj_t *container, const plant_data_t *data, uint32_t dirty);
} screen_desc_t;

static const screen_desc_t s_screens[SCREEN_COUNT] = {
    [SCREEN_MNEMONIC]    = { scr_mnemonic_create,    scr_mnemonic_update },
    [SCREEN_PARAMETERS]  = { scr_parameters_create,  scr_parameters_update },
    [SCREEN_WASHING]     = { scr_washing_create,     scr_washing_update },
    [SCREEN_ALARMS]      = { scr_alarms_create,      scr_alarms_update },
    [SCREEN_SETTINGS]    = { scr_settings_create,    scr_settings_update },
    [SCREEN_DIAGNOSTICS] = { scr_diagnostics_create, scr_diagnostics_update },
};

/** Маппинг ui_tab_id_t → screen_id_t. */
static const screen_id_t TAB_TO_SCREEN[UI_TAB_COUNT] = {
    [UI_TAB_MAIN]     = SCREEN_MNEMONIC,
    [UI_TAB_WASHING]  = SCREEN_WASHING,
    [UI_TAB_SETTINGS] = SCREEN_SETTINGS,
    [UI_TAB_DEBUG]    = SCREEN_DIAGNOSTICS,
};

/** Обратный маппинг screen_id_t → ui_tab_id_t (для подсветки активной
 *  вкладки). UI_TAB_COUNT означает «нет соответствующей вкладки» —
 *  активная подсветка не сбрасывается. */
static ui_tab_id_t screen_to_tab(screen_id_t s)
{
    for (int t = 0; t < UI_TAB_COUNT; t++) {
        if (TAB_TO_SCREEN[t] == s) return (ui_tab_id_t)t;
    }
    return UI_TAB_COUNT; /* sentinel */
}

static screen_id_t s_current     = SCREEN_MNEMONIC;
static lv_obj_t   *s_content     = NULL;     /* область между statusbar и tabbar */
static lv_obj_t   *s_statusbar   = NULL;
static lv_obj_t   *s_tabbar      = NULL;
static lv_obj_t   *s_screen_obj  = NULL;     /* корень текущего экрана */

/* ─── обработчик клика по вкладке ─────────────────────────────────── */
static void on_tab_select(ui_tab_id_t tab)
{
    if ((int)tab < 0 || tab >= UI_TAB_COUNT) return;
    ui_switch_screen(TAB_TO_SCREEN[tab]);
}

/* ─── callback пересоздания UI после смены темы ───────────────────── */
static void on_theme_change(void)
{
    /* TODO: полноценный rebuild — пересоздать statusbar/tabbar/content
     * и заново инициализировать тему LVGL. На данный момент новые цвета
     * подхватываются только при следующем create-цикле виджетов
     * (ui_switch_screen или после lv_refresh). */
    ESP_LOGI(TAG, "Theme changed — rebuild deferred (TODO)");
}

/* ─── refresh-таймер ──────────────────────────────────────────────── */
static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!plant_data_lock(5)) return;

    const plant_data_t *data = plant_data_get();
    uint32_t dirty = plant_data_get_and_clear_dirty();

    int64_t now = esp_timer_get_time();
    bool stale = (data->last_msg_time_us > 0)
                 && ((now - data->last_msg_time_us) > STALE_DATA_TIMEOUT_US);

    if (s_statusbar) {
        ui_statusbar_set_stale(s_statusbar, stale);
        ui_statusbar_update(s_statusbar, data);
    }

    if (s_screens[s_current].update && s_screen_obj) {
        s_screens[s_current].update(s_screen_obj, data, dirty);
    }

    plant_data_unlock();
}

/* ─── ui_init — фазы 1..5 ─────────────────────────────────────────── */
void ui_init(lv_display_t *disp)
{
    (void)disp;
    ESP_LOGI(TAG, "Initializing UI (statusbar+tabbar layout)");

    /* Фаза 1: тема LVGL + фон корневого экрана.
     * ui_theme_init() остаётся, но фон корня берётся из новых токенов. */
    lvgl_port_lock(0);
    /* Стартовая тема — light (UI_THEME_LIGHT по умолчанию). Можно сразу
     * прочитать из NVS и переключить на dark, если оператор её сохранял. */
    ui_tokens_set_theme(UI_THEME_LIGHT);
    ui_theme_init();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, ui_token_bg_base(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Фаза 2: statusbar (44px). */
    lvgl_port_lock(0);
    s_statusbar = ui_statusbar_create(scr, on_theme_change);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Фаза 3: контейнер контента (692px) + tabbar (64px). */
    lvgl_port_lock(0);
    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(s_content, 0, UI_STATUSBAR_H);
    lv_obj_set_style_bg_color(s_content, ui_token_bg_base(), 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_content, 0, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 0, 0);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    s_tabbar = ui_tabbar_create(scr, on_tab_select);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Фаза 4: стартовый экран — Диагностика (самый лёгкий по памяти,
     * быстро выводится). После запуска оператор переключается на Главную
     * через tabbar. */
    lvgl_port_lock(0);
    s_current = SCREEN_DIAGNOSTICS;
    s_screen_obj = s_screens[SCREEN_DIAGNOSTICS].create(s_content);
    ui_tabbar_set_active(s_tabbar, UI_TAB_DEBUG);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Фаза 5: refresh-таймер 250 мс. */
    lvgl_port_lock(0);
    lv_timer_create(refresh_timer_cb, 250, NULL);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "UI initialized");
}

/* ─── переключение экрана ─────────────────────────────────────────── */
void ui_switch_screen(screen_id_t id)
{
    if (id >= SCREEN_COUNT || id == s_current) return;

    lvgl_port_lock(0);

    if (s_screen_obj) {
        lv_obj_delete(s_screen_obj);
        s_screen_obj = NULL;
    }

    s_current = id;
    s_screen_obj = s_screens[id].create(s_content);

    /* Подсветить соответствующую вкладку (если есть в MVP-tabbar). */
    ui_tab_id_t tab = screen_to_tab(id);
    if (tab != UI_TAB_COUNT) {
        ui_tabbar_set_active(s_tabbar, tab);
    }

    lvgl_port_unlock();

    ESP_LOGI(TAG, "Switched to screen %d", id);
}
#endif /* !LVGL_LIVE_PREVIEW */
