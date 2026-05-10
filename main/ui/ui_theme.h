/**
 * @file ui_theme.h
 * @brief Тема оформления UI -- цветовая палитра и функции выбора цвета по состоянию.
 *
 * Определяет единую цветовую схему "Industrial Dark" для всех экранов.
 * Цвета разделены на группы: фон, текст, кнопки, состояния установки,
 * категории аварий, индикаторы насосов.
 */
#pragma once

#include "lvgl.h"
#include "ui_tokens.h"   /* источник истины для размеров и цветов */

/* === Цветовая палитра — алиасы на ui_token_*() для legacy-кода ===
 *
 * Старые экраны (scr_settings, scr_diagnostics, scr_alarms, scr_parameters,
 * ui_common::alarm_bar/nav_bar) используют эти макросы напрямую. Они
 * остаются для обратной совместимости, но теперь читают значения из
 * активной темы (light/dark) через ui_token_*() — при переключении
 * темы автоматически адаптируются.
 *
 * Новый код использовать ui_token_*() напрямую (см. ui_tokens.h §цвета).
 */
#define COLOR_BG_DARK         ui_token_bg_base()
#define COLOR_BG_PANEL        ui_token_bg_card()
#define COLOR_BG_WIDGET       ui_token_bg_elev()
#define COLOR_TEXT_PRIMARY    ui_token_text_primary()
#define COLOR_TEXT_SECONDARY  ui_token_text_secondary()
#define COLOR_TEXT_VALUE      ui_token_success()
#define COLOR_ACCENT          ui_token_accent()
#define COLOR_BTN_PRIMARY     ui_token_accent()
#define COLOR_BTN_DANGER      ui_token_danger()
#define COLOR_BTN_SUCCESS     ui_token_success()

/* === Цвета индикации состояния установки === */
#define COLOR_STATE_IDLE      ui_token_text_muted()
#define COLOR_STATE_AUTO      ui_token_success()
#define COLOR_STATE_WASHING   ui_token_info()
#define COLOR_STATE_MANUAL    ui_token_warning()
#define COLOR_STATE_FAULT     ui_token_danger()

/* === Цвета категорий аварий === */
#define COLOR_ALARM_CRITICAL  ui_token_danger()
#define COLOR_ALARM_ALARM     ui_token_danger()
#define COLOR_ALARM_WARNING   ui_token_warning()
#define COLOR_ALARM_INFO      ui_token_text_muted()

/* === Цвета индикаторов насосов на мнемосхеме === */
#define COLOR_PUMP_OFF        ui_token_bg_mute()
#define COLOR_PUMP_STARTING   ui_token_warning()
#define COLOR_PUMP_RUNNING    ui_token_accent()
#define COLOR_PUMP_FAULT      ui_token_danger()

/* Размеры основных UI-областей.
 *
 * Алиасы legacy-имён на актуальные UI_*_H/UI_SCREEN_W из ui_tokens.h.
 * Старые экраны (scr_*) ссылаются на эти имена — после перехода на
 * новый layout (statusbar 44px / tabbar 64px / content 692px) они
 * автоматически адаптируются. Новый код использовать UI_STATUSBAR_H,
 * UI_TABBAR_H, UI_CONTENT_H, UI_SCREEN_W напрямую.
 */
#define UI_ALARM_BAR_HEIGHT    UI_STATUSBAR_H   /* 44 (was 40) */
#define UI_NAV_BAR_HEIGHT      UI_TABBAR_H      /* 64 (was 60) */
#define UI_CONTENT_HEIGHT      UI_CONTENT_H     /* 692 (was 700) */
#define UI_SCREEN_WIDTH        UI_SCREEN_W      /* 1280 */

/**
 * Инициализация темы LVGL.
 * Создаёт тему по умолчанию с акцентным цветом и шрифтом 14pt,
 * устанавливает тёмный фон на активном экране.
 */
void ui_theme_init(void);

/**
 * Получение цвета по состоянию установки.
 *
 * @param state значение plant_state_t (IDLE/AUTO/WASHING/MANUAL/FAULT)
 * @return цвет LVGL для индикации данного состояния
 */
lv_color_t ui_state_color(int state);

/**
 * Получение цвета по категории аварии.
 *
 * @param cat значение alarm_category_t (CRITICAL/ALARM/WARNING/INFO)
 * @return цвет LVGL для индикации данной категории
 */
lv_color_t ui_alarm_cat_color(int cat);
