/**
 * @file ui_statusbar.h
 * @brief Верхняя статусная полоса (44px) — порт `.statusbar` из proto/index.html.
 *
 * Layout:
 *   [HH:MM:SS] [статус-сообщение flex-grow] [● Связь с хабом] [☾] [RU]
 *
 * Заменяет устаревший `alarm_bar` (40px). Высота — UI_STATUSBAR_H (см. ui_tokens.h).
 *
 * Цветовая семантика статус-сообщения:
 *   - default        → text-secondary
 *   - .is-warn       → warning  (есть активный warning)
 *   - .is-error      → danger   (активная авария или нет связи)
 *
 * При нажатии на ☾/☀ переключается тема (ui_tokens_set_theme + UI rebuild).
 * При нажатии на RU/EN переключается язык (lang_set + label refresh).
 *
 * См. docs/UI_SCREENS.md §1.1, docs/UI_SPEC.md §11 (theme/lang persistence).
 */
#pragma once

#include "lvgl.h"
#include "plant_data.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Callback переключения темы — вызывается после `ui_tokens_set_theme()`,
 *  когда нужно пересоздать экраны с новыми цветами. */
typedef void (*ui_statusbar_theme_cb_t)(void);

/**
 * Создание статус-полосы. Размещается в верхней части `parent` на y=0.
 *
 * @param parent      родительский объект (обычно lv_screen_active())
 * @param on_theme    callback для пересоздания UI при смене темы (может быть NULL)
 * @return            указатель на созданный объект полосы
 */
lv_obj_t *ui_statusbar_create(lv_obj_t *parent, ui_statusbar_theme_cb_t on_theme);

/**
 * Обновление содержимого по текущим данным установки.
 * Вызывается из refresh-таймера в ui_main.
 *
 * Поведение:
 *   - время: всегда текущее (HH:MM:SS)
 *   - статус: режим работы или код активной аварии (по приоритету)
 *   - индикатор соединения: ● зелёный (online) / ○ красный (offline)
 *
 * @param bar    указатель на объект полосы (можно NULL — работает через ctx)
 * @param data   текущий снимок plant_data_t (под локом)
 */
void ui_statusbar_update(lv_obj_t *bar, const plant_data_t *data);

/**
 * Установка флага «нет данных» (stale). Когда true, статус показывает
 * «НЕТ СВЯЗИ» в danger-цвете независимо от plant_data.
 */
void ui_statusbar_set_stale(lv_obj_t *bar, bool stale);

#ifdef __cplusplus
}
#endif
