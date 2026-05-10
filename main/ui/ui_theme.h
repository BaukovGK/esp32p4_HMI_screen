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

/* === Основная тёмная палитра === */
#define COLOR_BG_DARK       lv_color_hex(0x1A1A2E)  // Тёмный фон экрана
#define COLOR_BG_PANEL      lv_color_hex(0x16213E)  // Фон панелей (alarm bar, nav bar)
#define COLOR_BG_WIDGET     lv_color_hex(0x0F3460)  // Фон виджетов и кнопок
#define COLOR_TEXT_PRIMARY   lv_color_hex(0xE8E8E8)  // Основной текст (светло-серый)
#define COLOR_TEXT_SECONDARY lv_color_hex(0x8899AA)  // Вторичный текст (приглушённый)
#define COLOR_TEXT_VALUE     lv_color_hex(0x00FF88)  // Числовые значения (зелёный)
#define COLOR_ACCENT         lv_color_hex(0x00B4D8)  // Акцентный цвет (голубой)
#define COLOR_BTN_PRIMARY    lv_color_hex(0x0077B6)  // Основные кнопки (синий)
#define COLOR_BTN_DANGER     lv_color_hex(0xE63946)  // Кнопка "Нет" / опасное действие
#define COLOR_BTN_SUCCESS    lv_color_hex(0x2D6A4F)  // Кнопка "Да" / подтверждение

/* === Цвета индикации состояния установки === */
#define COLOR_STATE_IDLE     lv_color_hex(0x6C757D)  // ОЖИДАНИЕ -- серый
#define COLOR_STATE_AUTO     lv_color_hex(0x28A745)  // АВТО -- зелёный
#define COLOR_STATE_WASHING  lv_color_hex(0x007BFF)  // ПРОМЫВКА -- синий
#define COLOR_STATE_MANUAL   lv_color_hex(0xFFC107)  // РУЧНОЙ -- жёлтый
#define COLOR_STATE_FAULT    lv_color_hex(0xDC3545)  // АВАРИЯ -- красный

/* === Цвета категорий аварий === */
#define COLOR_ALARM_CRITICAL lv_color_hex(0xFF0000)  // Критическая -- ярко-красный
#define COLOR_ALARM_ALARM    lv_color_hex(0xFF8C00)  // Авария -- оранжевый
#define COLOR_ALARM_WARNING  lv_color_hex(0xFFD700)  // Предупреждение -- жёлтый
#define COLOR_ALARM_INFO     lv_color_hex(0x6C757D)  // Информация -- серый

/* === Цвета индикаторов насосов на мнемосхеме === */
#define COLOR_PUMP_OFF       lv_color_hex(0x4A4A4A)  // Насос выключен -- тёмно-серый
#define COLOR_PUMP_STARTING  lv_color_hex(0xFFD700)  // Насос запускается -- жёлтый
#define COLOR_PUMP_RUNNING   lv_color_hex(0x00FF88)  // Насос работает -- зелёный
#define COLOR_PUMP_FAULT     lv_color_hex(0xFF0000)  // Неисправность насоса -- красный

/* Размеры основных UI-областей (пиксели) */
#define UI_ALARM_BAR_HEIGHT    40
#define UI_NAV_BAR_HEIGHT      60
#define UI_CONTENT_HEIGHT      700
#define UI_SCREEN_WIDTH        1280

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
