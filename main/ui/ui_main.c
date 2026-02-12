/**
 * @file ui_main.c
 * @brief Главный модуль UI -- инициализация интерфейса и управление экранами.
 *
 * Модуль реализует:
 *  - Поэтапную инициализацию UI (тема, панель аварий, контент, навигация, таймер)
 *  - Таблицу дескрипторов экранов (create/update для каждого экрана)
 *  - Переключение между экранами с удалением предыдущего и созданием нового
 *  - Периодическое обновление данных через таймер LVGL (250 мс)
 *
 * Компиляция исключается при сборке для LVGL Live Preview.
 */
#ifndef LVGL_LIVE_PREVIEW
#include "ui_main.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "plant_data.h"
#include "board.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Заголовки всех экранов -- каждый экран предоставляет функции create и update
#include "scr_mnemonic.h"
#include "scr_parameters.h"
#include "scr_washing.h"
#include "scr_alarms.h"
#include "scr_settings.h"
#include "scr_diagnostics.h"

static const char *TAG = "ui_main";

/**
 * Дескриптор экрана: содержит указатели на функции создания и обновления.
 * Функция create создаёт виджеты экрана внутри родительского контейнера.
 * Функция update обновляет виджеты по данным plant_data_t.
 */
typedef struct {
    lv_obj_t *(*create)(lv_obj_t *parent);
    void      (*update)(lv_obj_t *container, const plant_data_t *data);
} screen_desc_t;

// Таблица дескрипторов всех экранов, индексируемая по screen_id_t
static const screen_desc_t s_screens[SCREEN_COUNT] = {
    [SCREEN_MNEMONIC]    = { scr_mnemonic_create,    scr_mnemonic_update },
    [SCREEN_PARAMETERS]  = { scr_parameters_create,  scr_parameters_update },
    [SCREEN_WASHING]     = { scr_washing_create,     scr_washing_update },
    [SCREEN_ALARMS]      = { scr_alarms_create,      scr_alarms_update },
    [SCREEN_SETTINGS]    = { scr_settings_create,    scr_settings_update },
    [SCREEN_DIAGNOSTICS] = { scr_diagnostics_create, scr_diagnostics_update },
};

static screen_id_t s_current = SCREEN_MNEMONIC;  // Текущий активный экран
static lv_obj_t   *s_content  = NULL;             // Контейнер контента (между alarm bar и nav bar)
static lv_obj_t   *s_alarm_bar = NULL;            // Верхняя панель аварий (40px)
static lv_obj_t   *s_nav_bar  = NULL;             // Нижняя навигационная панель (60px)
static lv_obj_t   *s_screen_obj = NULL;           // Корневой объект текущего экрана

/**
 * Обратный вызов навигационной панели.
 * Вызывается при нажатии кнопки навигации, передаёт id экрана.
 */
static void on_nav_select(int screen_id)
{
    if (screen_id >= 0 && screen_id < SCREEN_COUNT) {
        ui_switch_screen((screen_id_t)screen_id);
    }
}

/**
 * Callback таймера LVGL -- периодическое обновление данных на экране.
 * Вызывается каждые 250 мс. Блокирует мьютекс plant_data на чтение,
 * обновляет панель аварий и текущий активный экран.
 */
static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!plant_data_lock(5)) return; // Таймаут 5 мс -- пропускаем кадр при занятом мьютексе

    const plant_data_t *data = plant_data_get();

    // Панель аварий обновляется всегда, независимо от активного экрана
    ui_alarm_bar_update(s_alarm_bar, data);

    // Обновление виджетов текущего экрана
    if (s_screens[s_current].update && s_screen_obj) {
        s_screens[s_current].update(s_screen_obj, data);
    }

    plant_data_unlock();
}

/**
 * Инициализация пользовательского интерфейса.
 *
 * Выполняется в 5 фаз с vTaskDelay между ними для стабильности LVGL:
 *  1. Инициализация темы и фона экрана
 *  2. Создание панели аварий (верхняя полоса 40px)
 *  3. Создание контейнера контента (700px) и навигационной панели (60px)
 *  4. Создание первого экрана (Диагностика -- самый лёгкий для быстрого старта)
 *  5. Запуск таймера обновления данных (250 мс)
 */
void ui_init(lv_display_t *disp)
{
    (void)disp;
    ESP_LOGI(TAG, "Initializing UI");

    // Фаза 1: инициализация темы и тёмного фона
    lvgl_port_lock(0);
    ui_theme_init();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    // Фаза 2: панель аварий в верхней части экрана
    lvgl_port_lock(0);
    s_alarm_bar = ui_alarm_bar_create(scr);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    // Фаза 3: область контента и навигационная панель
    lvgl_port_lock(0);
    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, BOARD_DISP_H_RES, 700); // Высота = 800 - 40(alarm) - 60(nav)
    lv_obj_set_pos(s_content, 0, 40);                   // Ниже панели аварий
    lv_obj_set_style_bg_color(s_content, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_content, 0, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 0, 0);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    s_nav_bar = ui_nav_bar_create(scr, on_nav_select);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    // Фаза 4: стартовый экран -- Диагностика (самый лёгкий)
    lvgl_port_lock(0);
    s_current = SCREEN_DIAGNOSTICS;
    s_screen_obj = s_screens[SCREEN_DIAGNOSTICS].create(s_content);
    ui_nav_bar_set_active(s_nav_bar, SCREEN_DIAGNOSTICS);
    lvgl_port_unlock();
    vTaskDelay(pdMS_TO_TICKS(10));

    // Фаза 5: таймер обновления данных каждые 250 мс
    lvgl_port_lock(0);
    lv_timer_create(refresh_timer_cb, 250, NULL);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "UI initialized");
}

/**
 * Переключение на указанный экран.
 * Удаляет объект текущего экрана, создаёт новый из таблицы дескрипторов
 * и обновляет подсветку активной кнопки в навигационной панели.
 *
 * @param id идентификатор целевого экрана (из screen_id_t)
 */
void ui_switch_screen(screen_id_t id)
{
    if (id >= SCREEN_COUNT || id == s_current) return; // Игнорируем недопустимый или повторный экран

    lvgl_port_lock(0);

    // Удаление виджетов старого экрана
    if (s_screen_obj) {
        lv_obj_delete(s_screen_obj);
        s_screen_obj = NULL;
    }

    // Создание нового экрана в контейнере контента
    s_current = id;
    s_screen_obj = s_screens[id].create(s_content);

    // Подсветка активной вкладки навигации
    ui_nav_bar_set_active(s_nav_bar, id);

    lvgl_port_unlock();

    ESP_LOGI(TAG, "Switched to screen %d", id);
}
#endif /* !LVGL_LIVE_PREVIEW */
