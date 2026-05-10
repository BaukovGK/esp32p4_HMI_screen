/**
 * @file ui_sensor_modal.c
 * @brief Реализация sensor detail modal.
 *
 * Метаданные датчиков (sensor_meta_t) — прямой порт sensorMeta из
 * proto/app.js. Привязка по tag (ASCII только: P1..P4, Q1..Q4, S2/S3
 * для σ2/σ3 — потому что Greek σ нет в Montserrat fonts; в самом
 * mnemo они отображаются как σ2/σ3 через UTF-8, а в коде ищем по
 * латинской альтернативе).
 *
 * Если в будущем добавятся LV_FONT_MONTSERRAT_X с расширенным range —
 * можно вернуться к Unicode tags. Пока ищем по комбинации
 * (ASCII fallback OR UTF-8 σ-string).
 */
#include "ui_sensor_modal.h"
#include "ui_modal.h"
#include "ui_tokens.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ─── метаданные датчиков ─────────────────────────────────────────── */

typedef struct {
    /* Два варианта tag'а — для tolerant lookup. NULL во втором поле
     * = нет альтернативного написания. */
    const char *tag;
    const char *tag_alt;
    const char *name_ru;
    const char *name_en;
    const char *unit;
    float       range_min;
    float       range_max;
    /* Уставки. NAN → не задано. */
    float       warn_at;
    float       alarm_at;
    const char *modbus;
    const char *location_ru;
    const char *location_en;
} sensor_meta_t;

static const sensor_meta_t SENSOR_META[] = {
    {
        .tag = "P1", .tag_alt = NULL,
        .name_ru = "Давление после насоса преднагнетания",
        .name_en = "Pressure after pre-pump",
        .unit = "bar", .range_min = 0, .range_max = 6,
        .warn_at = 5.0f, .alarm_at = 5.5f,
        .modbus = "Slave 1 / AI1 / 4-20 mA",
        .location_ru = "после насоса преднагнетания (RO1)",
        .location_en = "after pre-pump (RO1)",
    },
    {
        .tag = "P2", .tag_alt = NULL,
        .name_ru = "Давление после фильтра мехочистки",
        .name_en = "Pressure after mechanical filter",
        .unit = "bar", .range_min = 0, .range_max = 6,
        .warn_at = 4.5f, .alarm_at = 5.0f,
        .modbus = "Slave 1 / AI2 / 4-20 mA",
        .location_ru = "после фильтра (контроль засорения)",
        .location_en = "after mechanical filter (clogging detection)",
    },
    {
        .tag = "P3", .tag_alt = NULL,
        .name_ru = "Давление после насоса 1-й ступени",
        .name_en = "Pressure after stage-1 pump",
        .unit = "bar", .range_min = 0, .range_max = 40,
        .warn_at = 32, .alarm_at = 35,
        .modbus = "Slave 1 / AI3 / 4-20 mA",
        .location_ru = "между насосом 1-й ст. и мембраной 1-й ст.",
        .location_en = "between stage-1 pump and membrane 1",
    },
    {
        .tag = "P4", .tag_alt = NULL,
        .name_ru = "Давление после насоса 2-й ступени",
        .name_en = "Pressure after stage-2 pump",
        .unit = "bar", .range_min = 0, .range_max = 10,
        .warn_at = 7.5f, .alarm_at = 8.0f,
        .modbus = "Slave 1 / AI4 / 4-20 mA",
        .location_ru = "между насосом 2-й ст. и мембраной 2-й ст.",
        .location_en = "between stage-2 pump and membrane 2",
    },
    {
        .tag = "Q1", .tag_alt = NULL,
        .name_ru = "Расход питательной воды",
        .name_en = "Feed water flow",
        .unit = "m3/h", .range_min = 0, .range_max = 5,
        .warn_at = NAN, .alarm_at = NAN,
        .modbus = "Slave 2 (URZH2KM) / ch.1",
        .location_ru = "после насоса преднагнетания",
        .location_en = "after pre-pump",
    },
    {
        .tag = "Q2", .tag_alt = NULL,
        .name_ru = "Расход рецикла 1-й ступени",
        .name_en = "Stage-1 recycle flow",
        .unit = "m3/h", .range_min = 0, .range_max = 3,
        .warn_at = NAN, .alarm_at = NAN,
        .modbus = "Slave 2 (URZH2KM) / ch.2",
        .location_ru = "труба рецикла концентрата к 1-й ст.",
        .location_en = "recycle pipe back to stage-1 pump",
    },
    {
        .tag = "Q3", .tag_alt = NULL,
        .name_ru = "Расход товарного пермеата",
        .name_en = "Product permeate flow",
        .unit = "m3/h", .range_min = 0, .range_max = 3,
        .warn_at = NAN, .alarm_at = NAN,
        .modbus = "Slave 2 (URZH2KM) / ch.3",
        .location_ru = "труба мембрана 2 - чистая ёмкость",
        .location_en = "membrane 2 to product tank pipe",
    },
    {
        .tag = "Q4", .tag_alt = NULL,
        .name_ru = "Расход рецикла 2-й ступени",
        .name_en = "Stage-2 recycle flow",
        .unit = "m3/h", .range_min = 0, .range_max = 2,
        .warn_at = NAN, .alarm_at = NAN,
        .modbus = "Slave 2 (URZH2KM) / ch.4",
        .location_ru = "труба рецикла концентрата к 2-й ст.",
        .location_en = "recycle pipe back to stage-2 pump",
    },
    {
        /* σ2 — кондуктометр пермеата 1-й ст. ASCII fallback "S2"
         * и UTF-8 вариант "σ2" (CF 83 32) — оба ищутся. */
        .tag = "S2", .tag_alt = "\xCF\x83" "2",
        .name_ru = "Проводимость пермеата 1-й ст.",
        .name_en = "Stage-1 permeate conductivity",
        .unit = "uS/cm", .range_min = 0, .range_max = 50,
        .warn_at = 30, .alarm_at = 40,
        .modbus = "Slave 10 (SL21 #1) / PERM1",
        .location_ru = "труба пермеата мембраны 1",
        .location_en = "membrane 1 permeate pipe",
    },
    {
        .tag = "S3", .tag_alt = "\xCF\x83" "3",
        .name_ru = "Проводимость товарного пермеата",
        .name_en = "Product permeate conductivity",
        .unit = "uS/cm", .range_min = 0, .range_max = 20,
        .warn_at = 5, .alarm_at = 10,
        .modbus = "Slave 11 (SL21 #2) / PERM2",
        .location_ru = "труба мембрана 2 - чистая ёмкость",
        .location_en = "membrane 2 to product tank pipe",
    },
};

#define SENSOR_META_COUNT (sizeof(SENSOR_META) / sizeof(SENSOR_META[0]))

/* ─── lookup по tag (учитываем UTF-8 alt) ────────────────────────── */

static const sensor_meta_t *find_meta(const char *tag)
{
    if (!tag) return NULL;
    for (size_t i = 0; i < SENSOR_META_COUNT; i++) {
        if (strcmp(SENSOR_META[i].tag, tag) == 0) return &SENSOR_META[i];
        if (SENSOR_META[i].tag_alt && strcmp(SENSOR_META[i].tag_alt, tag) == 0) {
            return &SENSOR_META[i];
        }
    }
    return NULL;
}

/* ─── классификация состояния по порогам ──────────────────────────── */

static ui_modal_state_t classify(float value, const sensor_meta_t *m)
{
    if (isnan(value)) return UI_MODAL_STATE_OFFLINE;
    if (!isnan(m->alarm_at) && value >= m->alarm_at) return UI_MODAL_STATE_DANGER;
    if (!isnan(m->warn_at)  && value >= m->warn_at)  return UI_MODAL_STATE_WARN;
    return UI_MODAL_STATE_OK;
}

static const char *state_label(ui_modal_state_t s)
{
    switch (s) {
    case UI_MODAL_STATE_OK:      return "НОРМА";
    case UI_MODAL_STATE_WARN:    return "ПРЕДУПРЕЖДЕНИЕ";
    case UI_MODAL_STATE_DANGER:  return "АВАРИЯ";
    case UI_MODAL_STATE_OFFLINE: return "НЕТ ДАННЫХ";
    default: return "?";
    }
}

/* ─── публичный API ───────────────────────────────────────────────── */

void ui_sensor_modal_show(const char *tag, float value)
{
    const sensor_meta_t *m = find_meta(tag);
    if (!m) return;

    /* Заголовок: "P3 - Давление после насоса 1-й ступени" */
    char title[128];
    snprintf(title, sizeof(title), "%s - %s", m->tag, m->name_ru);

    lv_obj_t *body = ui_modal_open(title, m->location_ru);
    if (!body) return;

    ui_modal_state_t state = classify(value, m);

    /* ─── Section 1: ТЕКУЩЕЕ ЗНАЧЕНИЕ ─────────────────────────── */
    lv_obj_t *s1 = ui_modal_add_section(body, "ТЕКУЩЕЕ ЗНАЧЕНИЕ");

    char value_str[16];
    if (isnan(value)) {
        snprintf(value_str, sizeof(value_str), "—");
    } else {
        snprintf(value_str, sizeof(value_str), "%.2f", value);
    }
    ui_modal_add_current_value(s1, value_str, m->unit, state, state_label(state));

    /* Range bar: pct = (value - min) / (max - min) * 100. */
    if (!isnan(value)) {
        int pct = 0;
        if (m->range_max > m->range_min) {
            float ratio = (value - m->range_min) / (m->range_max - m->range_min);
            pct = (int)(ratio * 100.0f);
        }
        char min_lbl[32], max_lbl[32];
        snprintf(min_lbl, sizeof(min_lbl), "%g %s", m->range_min, m->unit);
        snprintf(max_lbl, sizeof(max_lbl), "%g %s", m->range_max, m->unit);
        ui_modal_add_range_bar(s1, pct, min_lbl, max_lbl);
    }

    /* ─── Section 2: УСТАВКИ ─────────────────────────────────── */
    lv_obj_t *s2 = ui_modal_add_section(body, "УСТАВКИ");
    char buf[64];

    if (!isnan(m->warn_at)) {
        snprintf(buf, sizeof(buf), "< %g %s", m->warn_at, m->unit);
        ui_modal_add_prop_row(s2, "Норма", buf);
        snprintf(buf, sizeof(buf), "%g-%g %s",
                 m->warn_at,
                 isnan(m->alarm_at) ? m->range_max : m->alarm_at,
                 m->unit);
        ui_modal_add_prop_row(s2, "Предупреждение", buf);
    } else {
        snprintf(buf, sizeof(buf), "< %g %s", m->range_max, m->unit);
        ui_modal_add_prop_row(s2, "Норма", buf);
    }
    if (!isnan(m->alarm_at)) {
        snprintf(buf, sizeof(buf), "> %g %s", m->alarm_at, m->unit);
        ui_modal_add_prop_row(s2, "АВАРИЯ", buf);
    }

    /* ─── Section 3: ИСТОЧНИК ─────────────────────────────────── */
    lv_obj_t *s3 = ui_modal_add_section(body, "ИСТОЧНИК");
    ui_modal_add_prop_row(s3, "Modbus", m->modbus);
    ui_modal_add_prop_row(s3, "Обновлено", "сейчас");
}
