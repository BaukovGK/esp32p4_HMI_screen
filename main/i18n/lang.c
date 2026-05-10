/**
 * @file lang.c
 * @brief Реализация модуля интернационализации (i18n).
 *
 * Содержит внутреннее состояние (текущий язык) и логику доступа к строкам.
 * Доступ к s_current_lang защищён спинлоком (портмаксовый мьютекс), чтобы
 * гарантировать потокобезопасность при параллельном доступе из MQTT-callback
 * (lang_set) и UI-задачи (lang_get, lang_str). Локирование однооперационное
 * (чтение/запись uint8_t), минимальные накладные расходы.
 *
 * Язык сохраняется в NVS и восстанавливается при следующем запуске.
 */
#include "lang.h"
#include <stddef.h>
#ifndef LVGL_LIVE_PREVIEW
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
static const char *TAG = "lang";
#define LANG_NVS_NAMESPACE  "ro_settings"
#define LANG_NVS_KEY        "lang"
#endif

// Текущий выбранный язык (по умолчанию -- русский)
static lang_id_t s_current_lang = LANG_RU;

// Спинлок для защиты s_current_lang при одновременном доступе из разных потоков
#ifndef LVGL_LIVE_PREVIEW
static portMUX_TYPE s_lang_mux = portMUX_INITIALIZER_UNLOCKED;
#endif

// Массив указателей на таблицы строк для каждого языка.
// Индекс = lang_id_t, значение = указатель на массив const char*.
static const char **s_tables[LANG_COUNT] = {
    [LANG_EN] = NULL,   // заполняется в lang_init()
    [LANG_RU] = NULL,
};

/**
 * @brief Инициализация модуля i18n.
 *
 * Привязывает внешние массивы строк к внутренней таблице, загружает язык из NVS
 * (если доступен) или устанавливает fallback_lang. Должна вызываться до любых
 * обращений к lang_str().
 *
 * @param fallback_lang Язык, используемый если в NVS нет сохранённого языка.
 * @return Фактически выбранный язык (либо из NVS, либо fallback_lang).
 */
lang_id_t lang_init(lang_id_t fallback_lang)
{
    s_tables[LANG_EN] = lang_en_strings;  // регистрация английской таблицы
    s_tables[LANG_RU] = lang_ru_strings;  // регистрация русской таблицы

#ifndef LVGL_LIVE_PREVIEW
    /* Попытка загрузить сохранённый язык из NVS */
    nvs_handle_t nvs;
    if (nvs_open(LANG_NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t saved = 0;
        if (nvs_get_u8(nvs, LANG_NVS_KEY, &saved) == ESP_OK && saved < LANG_COUNT) {
            s_current_lang = (lang_id_t)saved;
            ESP_LOGI(TAG, "NVS: loaded language %d", saved);
            nvs_close(nvs);
            return s_current_lang;
        }
        nvs_close(nvs);
    }
#endif

    /* NVS не содержал валидного языка — используем fallback */
    s_current_lang = fallback_lang;
    return fallback_lang;
}

/**
 * @brief Установка текущего языка интерфейса.
 *
 * Потокобезопасна: может быть вызвана из MQTT-callback или UI-задачи.
 * Проверяет корректность параметра перед сменой языка.
 * После вызова UI-виджеты должны быть обновлены вручную
 * (модуль не рассылает уведомления об изменении языка).
 */
void lang_set(lang_id_t lang)
{
    if (lang < LANG_COUNT) {
#ifndef LVGL_LIVE_PREVIEW
        portENTER_CRITICAL(&s_lang_mux);
        s_current_lang = lang;
        portEXIT_CRITICAL(&s_lang_mux);

        /* Сохранение выбранного языка в NVS */
        nvs_handle_t nvs;
        if (nvs_open(LANG_NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_u8(nvs, LANG_NVS_KEY, (uint8_t)lang);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
#else
        s_current_lang = lang;
#endif
    }
}

/**
 * @brief Получение идентификатора текущего языка.
 *
 * Потокобезопасна: использует спинлок для защиты s_current_lang.
 *
 * @return Текущий язык (LANG_EN или LANG_RU).
 */
lang_id_t lang_get(void)
{
#ifndef LVGL_LIVE_PREVIEW
    portENTER_CRITICAL(&s_lang_mux);
    lang_id_t result = s_current_lang;
    portEXIT_CRITICAL(&s_lang_mux);
    return result;
#else
    return s_current_lang;
#endif
}

/**
 * @brief Получение локализованной строки по идентификатору.
 *
 * Потокобезопасна: использует спинлок при чтении s_current_lang.
 * Выполняет индексацию в таблице текущего языка. При некорректном
 * идентификаторе или если строка не задана (NULL), возвращает "???".
 *
 * @param id Строковый идентификатор из str_id_t.
 * @return UTF-8 строка на текущем языке или "???" при ошибке.
 */
const char *lang_str(str_id_t id)
{
    if (id >= STR_COUNT) return "???";

#ifndef LVGL_LIVE_PREVIEW
    portENTER_CRITICAL(&s_lang_mux);
    lang_id_t current = s_current_lang;
    portEXIT_CRITICAL(&s_lang_mux);
#else
    lang_id_t current = s_current_lang;
#endif

    const char *const *tbl = s_tables[current];
    if (!tbl) return "???";
    const char *s = tbl[id];
    return s ? s : "???";
}
