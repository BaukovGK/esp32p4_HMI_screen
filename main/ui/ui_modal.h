/**
 * @file ui_modal.h
 * @brief Generic modal-overlay infrastructure для HMI.
 *
 * Структура:
 *   backdrop (full-screen, semi-transparent black, click → close)
 *   └── card (centered, rounded, with title/subtitle/× close + body + footer)
 *       ├── header (title + subtitle + close button)
 *       ├── body   (scrollable, sections/rows добавляются caller'ом)
 *       └── footer ([Закрыть] button)
 *
 * API:
 *   ui_modal_open(...)  — создаёт и показывает overlay
 *   ui_modal_close(o)   — закрывает (удаляет overlay)
 *   ui_modal_add_section(o, title) — добавляет sm-section в body
 *   ui_modal_add_prop_row(parent, key, val) — добавляет key-value row
 *   ui_modal_add_current_value(parent, value, unit, state, state_label) — большое значение с badge
 *   ui_modal_add_range_bar(parent, range_min, range_max, value, unit) — индикатор положения в диапазоне
 *
 * Соответствует .sensor-modal и .equipment-modal из proto/style.css §sm-*.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Состояния для color-coding (норма/предупреждение/авария/нет данных). */
typedef enum {
    UI_MODAL_STATE_OK      = 0,
    UI_MODAL_STATE_WARN    = 1,
    UI_MODAL_STATE_DANGER  = 2,
    UI_MODAL_STATE_OFFLINE = 3,
} ui_modal_state_t;

/**
 * Открыть модал с заголовком/подзаголовком.
 *
 * @param title     Главная строка заголовка (UTF-8).
 * @param subtitle  Подзаголовок (UTF-8, может быть NULL).
 * @return          Указатель на body-container — туда caller добавляет
 *                  свои section'ы через ui_modal_add_section().
 *                  Сам overlay получается через lv_obj_get_screen(body)
 *                  → его удаление закрывает модал.
 */
lv_obj_t *ui_modal_open(const char *title, const char *subtitle);

/** Закрыть модал — body может быть body из ui_modal_open или сам overlay. */
void ui_modal_close(lv_obj_t *body_or_overlay);

/**
 * Добавить секцию с заголовком в body.
 * @return content container секции (туда добавлять rows / value blocks).
 */
lv_obj_t *ui_modal_add_section(lv_obj_t *body, const char *title);

/** Key-value row: "Modbus    Slave 1 · AI3". */
void ui_modal_add_prop_row(lv_obj_t *section, const char *key, const char *val);

/**
 * Большой блок "ТЕКУЩЕЕ ЗНАЧЕНИЕ":
 *   ┌──────────────────────────────────────┐
 *   │  26.8 bar           [Норма green]   │
 *   └──────────────────────────────────────┘
 *
 * @param state_label  Например "Норма" / "Предупреждение" / "АВАРИЯ" — текст badge'а
 */
void ui_modal_add_current_value(lv_obj_t *section,
                                 const char *value,
                                 const char *unit,
                                 ui_modal_state_t state,
                                 const char *state_label);

/**
 * Range bar — горизонтальная полоса 0..100% с указателем текущего значения.
 *
 * @param pct  Положение указателя в процентах (0..100).
 * @param min_label "0 bar" — подпись слева
 * @param max_label "40 bar" — подпись справа
 */
void ui_modal_add_range_bar(lv_obj_t *section, int pct,
                            const char *min_label, const char *max_label);

/**
 * Progress bar — заполненная полоса 0..pct% с цветом по состоянию
 * (для "наработка картриджа" / "до промывки" и т.п.).
 *
 * @param pct    Заполнение в процентах.
 * @param state  Цвет заливки (OK=accent green, WARN=warning, DANGER=danger).
 */
void ui_modal_add_progress_bar(lv_obj_t *section, int pct, ui_modal_state_t state);

#ifdef __cplusplus
}
#endif
