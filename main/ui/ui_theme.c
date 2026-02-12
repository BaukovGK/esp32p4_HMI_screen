/**
 * @file ui_theme.c
 * @brief Инициализация темы LVGL и функции маппинга состояний на цвета.
 *
 * Модуль создаёт тёмную тему ("Industrial Dark") на базе стандартной
 * темы LVGL v9 с пользовательскими акцентными цветами и шрифтом.
 * Предоставляет функции для получения цвета по состоянию установки
 * и категории аварии -- используются во всех экранах.
 */
#include "ui_theme.h"
#include "ui_fonts.h"
#include "plant_data.h"

/**
 * Инициализация темы LVGL.
 * Создаёт тему по умолчанию (тёмный режим) с акцентным голубым цветом
 * и шрифтом Montserrat 14pt. Устанавливает тёмный фон на активном экране.
 */
void ui_theme_init(void)
{
    lv_display_t *disp = lv_display_get_default();
    if (!disp) return;

    // Инициализация стандартной тёмной темы LVGL (dark = true)
    lv_theme_t *theme = lv_theme_default_init(disp,
        COLOR_ACCENT, COLOR_BTN_PRIMARY, true, UI_FONT_14);
    lv_display_set_theme(disp, theme);

    // Установка тёмного фона на активном экране
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

/**
 * Получение цвета по состоянию установки обратного осмоса.
 * Маппинг: IDLE=серый, AUTO=зелёный, WASHING=синий, MANUAL=жёлтый, FAULT=красный.
 *
 * @param state значение plant_state_t
 * @return цвет LVGL для индикации состояния
 */
lv_color_t ui_state_color(int state)
{
    switch (state) {
    case PLANT_STATE_IDLE:    return COLOR_STATE_IDLE;     // Серый -- ожидание
    case PLANT_STATE_AUTO:    return COLOR_STATE_AUTO;     // Зелёный -- автоматический
    case PLANT_STATE_WASHING: return COLOR_STATE_WASHING;  // Синий -- промывка
    case PLANT_STATE_MANUAL:  return COLOR_STATE_MANUAL;   // Жёлтый -- ручной
    case PLANT_STATE_FAULT:   return COLOR_STATE_FAULT;    // Красный -- авария
    default:                  return COLOR_STATE_IDLE;     // По умолчанию -- серый
    }
}

/**
 * Получение цвета по категории аварии.
 * Маппинг: CRITICAL=красный, ALARM=оранжевый, WARNING=жёлтый, INFO=серый.
 *
 * @param cat значение alarm_category_t
 * @return цвет LVGL для индикации категории аварии
 */
lv_color_t ui_alarm_cat_color(int cat)
{
    switch (cat) {
    case ALARM_CAT_CRITICAL: return COLOR_ALARM_CRITICAL;  // Красный -- критическая
    case ALARM_CAT_ALARM:    return COLOR_ALARM_ALARM;     // Оранжевый -- авария
    case ALARM_CAT_WARNING:  return COLOR_ALARM_WARNING;   // Жёлтый -- предупреждение
    default:                 return COLOR_ALARM_INFO;      // Серый -- информация
    }
}
