/**
 * @file lang.c
 * @brief Реализация модуля интернационализации (i18n).
 *
 * Содержит внутреннее состояние (текущий язык) и логику доступа к строкам.
 * Модуль не является потокобезопасным -- предполагается, что переключение
 * языка и чтение строк выполняются из одного потока (LVGL-задача).
 *
 * Язык сохраняется в NVS и восстанавливается при следующем запуске.
 */
#include "lang.h"
#include <stddef.h>
#ifndef LVGL_LIVE_PREVIEW
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
static const char *TAG = "lang";
#define LANG_NVS_NAMESPACE  "ro_settings"
#define LANG_NVS_KEY        "lang"
#endif

// Текущий выбранный язык (по умолчанию -- русский)
static lang_id_t s_current_lang = LANG_RU;

// Массив указателей на таблицы строк для каждого языка.
// Индекс = lang_id_t, значение = указатель на массив const char*.
static const char **s_tables[LANG_COUNT] = {
    [LANG_EN] = NULL,   // заполняется в lang_init()
    [LANG_RU] = NULL,
};

/**
 * @brief Инициализация модуля i18n.
 *
 * Привязывает внешние массивы строк к внутренней таблице и выбирает
 * язык по умолчанию. Должна вызываться до любых обращений к lang_str().
 */
void lang_init(lang_id_t default_lang)
{
    s_tables[LANG_EN] = lang_en_strings;  // регистрация английской таблицы
    s_tables[LANG_RU] = lang_ru_strings;  // регистрация русской таблицы
    s_current_lang = default_lang;

#ifndef LVGL_LIVE_PREVIEW
    /* Попытка загрузить сохранённый язык из NVS */
    nvs_handle_t nvs;
    if (nvs_open(LANG_NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t saved = 0;
        if (nvs_get_u8(nvs, LANG_NVS_KEY, &saved) == ESP_OK && saved < LANG_COUNT) {
            s_current_lang = (lang_id_t)saved;
            ESP_LOGI(TAG, "NVS: loaded language %d", saved);
        }
        nvs_close(nvs);
    }
#endif
}

/**
 * @brief Установка текущего языка интерфейса.
 *
 * Проверяет корректность параметра перед сменой языка.
 * После вызова UI-виджеты должны быть обновлены вручную
 * (модуль не рассылает уведомления об изменении языка).
 */
void lang_set(lang_id_t lang)
{
    if (lang < LANG_COUNT) {
        s_current_lang = lang;  // защита от выхода за границы массива

#ifndef LVGL_LIVE_PREVIEW
        /* Сохранение выбранного языка в NVS */
        nvs_handle_t nvs;
        if (nvs_open(LANG_NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_u8(nvs, LANG_NVS_KEY, (uint8_t)lang);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
#endif
    }
}

/**
 * @brief Получение идентификатора текущего языка.
 * @return Текущий язык (LANG_EN или LANG_RU).
 */
lang_id_t lang_get(void)
{
    return s_current_lang;
}

/**
 * @brief Получение локализованной строки по идентификатору.
 *
 * Выполняет индексацию в таблице текущего языка. При некорректном
 * идентификаторе или если строка не задана (NULL), возвращает "???".
 *
 * @param id Строковый идентификатор из str_id_t.
 * @return UTF-8 строка на текущем языке или "???" при ошибке.
 */
const char *lang_str(str_id_t id)
{
    if (id >= STR_COUNT) return "???";
    const char *const *tbl = s_tables[s_current_lang];
    if (!tbl) return "???";
    const char *s = tbl[id];
    return s ? s : "???";
}
