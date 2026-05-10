/**
 * @file ui_tokens.h
 * @brief Дизайн-токены HMI: цвета, отступы, размеры, типографика.
 *
 * Прямой порт CSS-переменных из proto/style.css. Каждый токен здесь
 * соответствует одному `--name` в прототипе. Используйте accessor-функции
 * `ui_token_*()` вместо прямого обращения к таблице — это позволит
 * переключать light/dark темы в runtime через `ui_tokens_set_theme()`.
 *
 * Соответствие токенов:
 *   ui_token_bg_base()       ↔ var(--bg-base)
 *   ui_token_bg_card()       ↔ var(--bg-card)
 *   ui_token_accent()        ↔ var(--accent)
 *   ui_token_water()         ↔ var(--water)
 *   ...
 *
 * Спецификация: docs/UI_TOKENS.md.
 */
#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ───── темы ──────────────────────────────────────────────────────── */

typedef enum {
    UI_THEME_LIGHT = 0,
    UI_THEME_DARK  = 1,
} ui_theme_id_t;

/**
 * Активировать тему (light/dark). Все последующие вызовы `ui_token_*()`
 * вернут значения соответствующей темы. Не перерисовывает существующие
 * виджеты — для применения нужно пересоздать экраны или обновить стили.
 */
void ui_tokens_set_theme(ui_theme_id_t theme);
ui_theme_id_t ui_tokens_get_theme(void);

/* ───── цветовые токены ───────────────────────────────────────────── */
/* Семантика: см. docs/UI_TOKENS.md §2 «Цветовая палитра». */

/* Поверхности */
lv_color_t ui_token_bg_base(void);     /* фон экрана за всеми панелями */
lv_color_t ui_token_bg_card(void);     /* фон карточек, модалов, tank-shell */
lv_color_t ui_token_bg_mute(void);     /* приглушённый фон (filter-bg, info-strip) */
lv_color_t ui_token_bg_elev(void);     /* приподнятый фон (hover/active state) */
lv_color_t ui_token_border(void);      /* стандартная граница 1px */
lv_color_t ui_token_border_strong(void); /* акцентированная граница */

/* Текст */
lv_color_t ui_token_text_primary(void);
lv_color_t ui_token_text_secondary(void);
lv_color_t ui_token_text_muted(void);
lv_color_t ui_token_text_inverse(void); /* текст поверх accent (на кнопках) */

/* Бренд */
lv_color_t ui_token_accent(void);          /* корпоративный зелёный НТТ */
lv_color_t ui_token_accent_hover(void);
lv_color_t ui_token_accent_mute(void);     /* light-tint для активных состояний */

/* Состояния */
lv_color_t ui_token_success(void);
lv_color_t ui_token_warning(void);
lv_color_t ui_token_danger(void);
lv_color_t ui_token_info(void);            /* для рециклов и информационных меток */

/* Технологические */
lv_color_t ui_token_pipe(void);            /* серый — труба не активна */
lv_color_t ui_token_pipe_active(void);     /* активная труба (= accent) */
lv_color_t ui_token_tank_stroke(void);     /* контур ёмкости */
lv_color_t ui_token_water(void);           /* цвет воды в ёмкости (#38bdf8) */

/* ───── отступы ───────────────────────────────────────────────────── */
/* Прямой порт из proto/style.css: --gap-xs..--gap-2xl */

#define UI_GAP_XS    4    /* var(--gap-xs)  */
#define UI_GAP_SM    8    /* var(--gap-sm)  */
#define UI_GAP_MD    12   /* var(--gap-md)  */
#define UI_GAP_LG    16   /* var(--gap-lg)  */
#define UI_GAP_XL    24   /* var(--gap-xl)  */
#define UI_GAP_2XL   32   /* var(--gap-2xl) */

/* ───── скругления ────────────────────────────────────────────────── */

#define UI_RADIUS_SM   4    /* var(--radius-sm) */
#define UI_RADIUS_MD   8    /* var(--radius-md) */
#define UI_RADIUS_LG   12   /* var(--radius-lg) */

/* ───── размеры экрана ────────────────────────────────────────────── */
/* Waveshare ESP32-P4-NANO 7" — 1280×800. Layout как в прототипе:
 * statusbar 44px / content 692px / tabbar 64px = 800. */

#define UI_SCREEN_W      1280
#define UI_SCREEN_H      800
#define UI_STATUSBAR_H   44
#define UI_TABBAR_H      64
#define UI_CONTENT_H     (UI_SCREEN_H - UI_STATUSBAR_H - UI_TABBAR_H)
#define UI_TOUCH_MIN     60   /* минимальная высота кнопки для тач (var(--touch-min)) */

/* ───── шрифтовая шкала ───────────────────────────────────────────── */
/* Привязка к ui_fonts.h (Montserrat). Размеры из прототипа:
 *   --fs-xs   12px   подписи единиц
 *   --fs-sm   14px   secondary text
 *   --fs-md   16px   body
 *   --fs-lg   20px   значения, sm headings
 *   --fs-xl   28px   primary values
 *   --fs-2xl  36px   hero values
 *   --fs-3xl  48px   alarms */

#define UI_FONT_XS    UI_FONT_12
#define UI_FONT_SM    UI_FONT_14
#define UI_FONT_MD    UI_FONT_16
#define UI_FONT_LG    UI_FONT_20
#define UI_FONT_XL    UI_FONT_28
#define UI_FONT_2XL   UI_FONT_36
/* UI_FONT_3XL (48px) — TODO: добавить Montserrat_48_4.c, либо использовать
 * lv_font_montserrat_48 из стандартной поставки LVGL. */

/* ───── pump-circle — состояния насоса (для scr_mnemonic) ─────────── */
/* Семантика и анимации: см. docs/UI_TOKENS.md §8.2.1 + UI_SPEC.md §9.5. */

typedef enum {
    UI_PUMP_OFF      = 0,   /* серый, статичный */
    UI_PUMP_RUNNING  = 1,   /* зелёный градиент, ротор крутится 1.5s */
    UI_PUMP_STARTING = 2,   /* жёлтый, медленное вращение 3s + пульс 1.2s */
    UI_PUMP_ERROR    = 3,   /* красный, статичный ротор + пульс 1.0s */
} ui_pump_state_t;

/**
 * Цвета корпуса насоса по состоянию.
 * Для running — заливка делается gradient'ом (LVGL: lv_grad_dsc),
 * остальные — solid fill.
 */
lv_color_t ui_token_pump_bg(ui_pump_state_t state);
lv_color_t ui_token_pump_stroke(ui_pump_state_t state);
lv_color_t ui_token_pump_blade(ui_pump_state_t state);

/* ───── level-sw — поплавковый датчик уровня (для tank-graphics) ──── */

typedef enum {
    UI_LEVEL_SW_DRY    = 0,   /* сухой — hollow gray */
    UI_LEVEL_SW_ACTIVE = 1,   /* мокрый — filled accent green */
    UI_LEVEL_SW_ALARM  = 2,   /* авария — pulsating danger red */
} ui_level_sw_state_t;

lv_color_t ui_token_level_sw_fill(ui_level_sw_state_t state);
lv_color_t ui_token_level_sw_stroke(ui_level_sw_state_t state);

#ifdef __cplusplus
}
#endif
