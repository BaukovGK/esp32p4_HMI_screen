/**
 * @file ui_fonts.h
 * @brief Декларации шрифтов UI и удобные макросы-псевдонимы.
 *
 * Все шрифты -- Montserrat с глубиной антиалиасинга 4 бита (16 уровней).
 * Суффикс "_4" в имени обозначает bpp (bits per pixel) для глифов.
 * Шрифты включают символы Latin + Cyrillic для двуязычного интерфейса (RU/EN).
 *
 * Размеры подобраны для дисплея 10.1" (1280x800, ~150 DPI):
 *  - 12-14pt: мелкие подписи, единицы измерения
 *  - 16-18pt: вторичный текст, табличные данные
 *  - 20-22pt: основной текст, названия параметров
 *  - 24-28pt: заголовки экранов, крупные значения
 *  - 36pt: ключевые показатели на мнемосхеме
 */
#pragma once

#include "lvgl.h"

// Декларации внешних шрифтов (определены в сгенерированных .c файлах)
LV_FONT_DECLARE(Montserrat_12_4);
LV_FONT_DECLARE(Montserrat_14_4);
LV_FONT_DECLARE(Montserrat_16_4);
LV_FONT_DECLARE(Montserrat_18_4);
LV_FONT_DECLARE(Montserrat_20_4);
LV_FONT_DECLARE(Montserrat_22_4);
LV_FONT_DECLARE(Montserrat_24_4);
LV_FONT_DECLARE(Montserrat_28_4);
LV_FONT_DECLARE(Montserrat_36_4);

// Макросы-псевдонимы для удобного использования в коде UI (UI_FONT_XX)
#define UI_FONT_12  &Montserrat_12_4   // 12pt -- мелкие подписи
#define UI_FONT_14  &Montserrat_14_4   // 14pt -- шрифт по умолчанию для темы
#define UI_FONT_16  &Montserrat_16_4   // 16pt -- вторичный текст
#define UI_FONT_18  &Montserrat_18_4   // 18pt -- табличные данные
#define UI_FONT_20  &Montserrat_20_4   // 20pt -- основной текст
#define UI_FONT_22  &Montserrat_22_4   // 22pt -- названия параметров, диалоги
#define UI_FONT_24  &Montserrat_24_4   // 24pt -- заголовки
#define UI_FONT_28  &Montserrat_28_4   // 28pt -- крупные значения
#define UI_FONT_36  &Montserrat_36_4   // 36pt -- ключевые показатели
