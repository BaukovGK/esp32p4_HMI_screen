/**
 * @file ui_equipment_modal.c
 * @brief Реализация equipment detail modal (filter/pump/membrane).
 *
 * Метаданные — static таблицы, лидирующий поиск по id-строке.
 * Каждый kind (filter/pump/membrane) имеет свою render-функцию.
 *
 * TODO: на целевом устройстве значения (runtime_h, state, etc.)
 * будут браться из plant_data_t. Пока — захардкоженные mock-данные
 * (1:1 с proto/app.js equipmentMeta) для демонстрации UI.
 */
#include "ui_equipment_modal.h"
#include "ui_modal.h"
#include "ui_tokens.h"
#include <stdio.h>
#include <string.h>

/* ─── формат-helper'ы ─────────────────────────────────────────────── */

/** "1284 ч (53 дн)" или "8.4 ч" для часов < 24. */
static void format_hours(char *buf, size_t bufsz, float hours)
{
    if (hours < 24.0f) {
        snprintf(buf, bufsz, "%.1f ч", hours);
    } else {
        int total = (int)hours;
        int days = total / 24;
        snprintf(buf, bufsz, "%d ч (%d дн)", total, days);
    }
}

/* ─── FILTER ──────────────────────────────────────────────────────── */

typedef struct {
    const char *name;
    const char *subtitle;
    float       delta_p;          /* bar */
    float       delta_p_warn;
    float       delta_p_alarm;
    float       runtime_h;        /* часы наработки картриджа */
    float       runtime_max_h;    /* ресурс */
    const char *last_replaced;
} filter_meta_t;

static const filter_meta_t FILTER_META = {
    .name = "Фильтр механической очистки",
    .subtitle = "5 мкм картридж",
    .delta_p = 0.32f,
    .delta_p_warn = 0.5f,
    .delta_p_alarm = 0.8f,
    .runtime_h = 1284,
    .runtime_max_h = 2000,
    .last_replaced = "2026-03-15",
};

static ui_modal_state_t classify_dp(float dp, const filter_meta_t *m)
{
    if (dp >= m->delta_p_alarm) return UI_MODAL_STATE_DANGER;
    if (dp >= m->delta_p_warn)  return UI_MODAL_STATE_WARN;
    return UI_MODAL_STATE_OK;
}

static const char *modal_state_label(ui_modal_state_t s)
{
    switch (s) {
    case UI_MODAL_STATE_OK:      return "НОРМА";
    case UI_MODAL_STATE_WARN:    return "ПРЕДУПРЕЖДЕНИЕ";
    case UI_MODAL_STATE_DANGER:  return "АВАРИЯ";
    case UI_MODAL_STATE_OFFLINE: return "НЕТ ДАННЫХ";
    default: return "?";
    }
}

static void render_filter_modal(void)
{
    const filter_meta_t *m = &FILTER_META;
    lv_obj_t *body = ui_modal_open("Фильтр", m->subtitle);
    if (!body) return;

    /* Section 1: ΔP — перепад давления (P1 − P2). */
    lv_obj_t *s1 = ui_modal_add_section(body, "ΔP — перепад давления (P1 − P2)");
    char val[16], buf[64];
    ui_modal_state_t dp_state = classify_dp(m->delta_p, m);

    snprintf(val, sizeof(val), "%.2f", m->delta_p);
    ui_modal_add_current_value(s1, val, "bar", dp_state, modal_state_label(dp_state));

    snprintf(buf, sizeof(buf), "< %.1f bar", m->delta_p_warn);
    ui_modal_add_prop_row(s1, "Норма", buf);
    snprintf(buf, sizeof(buf), "%.1f-%.1f bar", m->delta_p_warn, m->delta_p_alarm);
    ui_modal_add_prop_row(s1, "Предупреждение", buf);
    snprintf(buf, sizeof(buf), "> %.1f bar", m->delta_p_alarm);
    ui_modal_add_prop_row(s1, "АВАРИЯ (засорение)", buf);

    /* Section 2: Наработка картриджа. */
    lv_obj_t *s2 = ui_modal_add_section(body, "Наработка картриджа");

    int run_pct = (int)((m->runtime_h * 100.0f) / m->runtime_max_h);
    ui_modal_state_t run_state =
        (run_pct >= 95) ? UI_MODAL_STATE_DANGER :
        (run_pct >= 80) ? UI_MODAL_STATE_WARN   : UI_MODAL_STATE_OK;

    char ph[32], pm[32];
    format_hours(ph, sizeof(ph), m->runtime_h);
    format_hours(pm, sizeof(pm), m->runtime_max_h);
    snprintf(buf, sizeof(buf), "%s / %s", ph, pm);
    ui_modal_add_prop_row(s2, "Использовано", buf);

    ui_modal_add_progress_bar(s2, run_pct, run_state);

    float remaining = m->runtime_max_h - m->runtime_h;
    if (remaining < 0) remaining = 0;
    format_hours(buf, sizeof(buf), remaining);
    ui_modal_add_prop_row(s2, "До замены", buf);
    ui_modal_add_prop_row(s2, "Установлен", m->last_replaced);
}

/* ─── PUMP ────────────────────────────────────────────────────────── */

typedef enum {
    PMP_RUNNING = 0,
    PMP_STARTING,
    PMP_ERROR,
    PMP_OFF,
} pump_state_e;

typedef struct {
    const char *id;
    const char *name;
    const char *subtitle;       /* "RO1 / DI6" — modbus + DI */
    const char *model;
    const char *rated_q;
    const char *rated_p;
    pump_state_e state;
    float       run_continuous_h;
    float       run_total_h;
    int         starts;
    const char *last_start;
} pump_meta_t;

static const pump_meta_t PUMP_META[] = {
    {
        .id = "pump-pre", .name = "Преднагнетательный насос",
        .subtitle = "RO1 / DI6",
        .model = "Grundfos CR 3-4", .rated_q = "4 m3/h", .rated_p = "4 bar",
        .state = PMP_RUNNING,
        .run_continuous_h = 8.4f, .run_total_h = 4521, .starts = 1247,
        .last_start = "2026-05-11 06:08",
    },
    {
        .id = "pump-st1", .name = "Насос 1-й ступени",
        .subtitle = "RO2 / DI7",
        .model = "Grundfos BMG 4-15", .rated_q = "4 m3/h", .rated_p = "30 bar",
        .state = PMP_RUNNING,
        .run_continuous_h = 8.4f, .run_total_h = 4380, .starts = 1108,
        .last_start = "2026-05-11 06:08",
    },
    {
        .id = "pump-st2", .name = "Насос 2-й ступени",
        .subtitle = "RO3 / DI8",
        .model = "Grundfos CRN 5-12", .rated_q = "2.5 m3/h", .rated_p = "8 bar",
        .state = PMP_RUNNING,
        .run_continuous_h = 8.4f, .run_total_h = 4290, .starts = 1095,
        .last_start = "2026-05-11 06:08",
    },
};
#define PUMP_META_COUNT (sizeof(PUMP_META) / sizeof(PUMP_META[0]))

static const char *pump_state_text(pump_state_e s)
{
    switch (s) {
    case PMP_RUNNING:  return "РАБОТАЕТ";
    case PMP_STARTING: return "ПУСК";
    case PMP_ERROR:    return "АВАРИЯ";
    case PMP_OFF:      return "ОСТАНОВЛЕН";
    default: return "?";
    }
}

static ui_modal_state_t pump_modal_state(pump_state_e s)
{
    switch (s) {
    case PMP_RUNNING:  return UI_MODAL_STATE_OK;
    case PMP_STARTING: return UI_MODAL_STATE_WARN;
    case PMP_ERROR:    return UI_MODAL_STATE_DANGER;
    case PMP_OFF:
    default:           return UI_MODAL_STATE_OFFLINE;
    }
}

static void render_pump_modal(const pump_meta_t *m)
{
    if (!m) return;
    char title[128];
    snprintf(title, sizeof(title), "%s", m->name);
    char sub[64];
    snprintf(sub, sizeof(sub), "%s / %s", m->subtitle, m->model);

    lv_obj_t *body = ui_modal_open(title, sub);
    if (!body) return;

    /* Section 1: Состояние. */
    lv_obj_t *s1 = ui_modal_add_section(body, "Состояние");
    ui_modal_state_t st = pump_modal_state(m->state);
    /* Передаём пустой unit, NULL state_label чтобы badge не показывался —
     * value сам по себе цветной по state. */
    ui_modal_add_current_value(s1, pump_state_text(m->state), NULL, st, NULL);

    /* Section 2: Время работы. */
    lv_obj_t *s2 = ui_modal_add_section(body, "Время работы");
    char buf[64];
    format_hours(buf, sizeof(buf), m->run_continuous_h);
    ui_modal_add_prop_row(s2, "В текущем цикле", buf);
    ui_modal_add_prop_row(s2, "Запуск цикла", m->last_start);
    format_hours(buf, sizeof(buf), m->run_total_h);
    ui_modal_add_prop_row(s2, "Общая наработка", buf);
    snprintf(buf, sizeof(buf), "%d", m->starts);
    ui_modal_add_prop_row(s2, "Количество пусков", buf);

    /* Section 3: Характеристики. */
    lv_obj_t *s3 = ui_modal_add_section(body, "Характеристики");
    ui_modal_add_prop_row(s3, "Модель", m->model);
    ui_modal_add_prop_row(s3, "Q ном", m->rated_q);
    ui_modal_add_prop_row(s3, "P ном", m->rated_p);
}

static const pump_meta_t *find_pump(const char *id)
{
    for (size_t i = 0; i < PUMP_META_COUNT; i++) {
        if (strcmp(PUMP_META[i].id, id) == 0) return &PUMP_META[i];
    }
    return NULL;
}

/* ─── MEMBRANE ────────────────────────────────────────────────────── */

typedef struct {
    const char *id;
    const char *name;
    const char *subtitle;
    const char *type_name;
    float       rejection;                /* % NaCl */
    const char *last_wash;
    float       run_since_wash_h;
    float       run_since_wash_warn_h;    /* плановая после ~720 ч */
    float       run_total_h;
    float       run_max_h;                /* ресурс */
} membrane_meta_t;

static const membrane_meta_t MEMBRANE_META[] = {
    {
        .id = "membrane-1", .name = "Мембрана 1-й ступени",
        .subtitle = "2 элемента 4040",
        .type_name = "RO модуль (BW)",
        .rejection = 99.2f,
        .last_wash = "2026-04-12",
        .run_since_wash_h = 678, .run_since_wash_warn_h = 720,
        .run_total_h = 9412, .run_max_h = 20000,
    },
    {
        .id = "membrane-2", .name = "Мембрана 2-й ступени",
        .subtitle = "2 элемента 4040",
        .type_name = "RO модуль (BW)",
        .rejection = 99.5f,
        .last_wash = "2026-04-12",
        .run_since_wash_h = 678, .run_since_wash_warn_h = 720,
        .run_total_h = 9412, .run_max_h = 20000,
    },
};
#define MEMBRANE_META_COUNT (sizeof(MEMBRANE_META) / sizeof(MEMBRANE_META[0]))

static void render_membrane_modal(const membrane_meta_t *m)
{
    if (!m) return;
    lv_obj_t *body = ui_modal_open(m->name, m->subtitle);
    if (!body) return;

    char buf[64], val[16];

    /* Section 1: Селективность по NaCl. */
    lv_obj_t *s1 = ui_modal_add_section(body, "Селективность (NaCl rejection)");
    snprintf(val, sizeof(val), "%.1f", m->rejection);
    ui_modal_add_current_value(s1, val, "%", UI_MODAL_STATE_OK, "НОРМА");

    /* Section 2: Наработка от последней промывки. */
    lv_obj_t *s2 = ui_modal_add_section(body, "Наработка от последней промывки");
    int wash_pct = (int)((m->run_since_wash_h * 100.0f) / m->run_since_wash_warn_h);
    ui_modal_state_t wash_state =
        (wash_pct >= 100) ? UI_MODAL_STATE_DANGER :
        (wash_pct >= 80)  ? UI_MODAL_STATE_WARN   : UI_MODAL_STATE_OK;

    char ph[32], pm[32];
    format_hours(ph, sizeof(ph), m->run_since_wash_h);
    format_hours(pm, sizeof(pm), m->run_since_wash_warn_h);
    snprintf(buf, sizeof(buf), "%s / %s", ph, pm);
    ui_modal_add_prop_row(s2, "Прошло", buf);
    ui_modal_add_progress_bar(s2, wash_pct > 100 ? 100 : wash_pct, wash_state);

    ui_modal_add_prop_row(s2, "Последняя промывка", m->last_wash);
    float wash_remaining = m->run_since_wash_warn_h - m->run_since_wash_h;
    if (wash_remaining < 0) wash_remaining = 0;
    format_hours(buf, sizeof(buf), wash_remaining);
    ui_modal_add_prop_row(s2, "До плановой промывки", buf);

    /* Section 3: Общее время работы (ресурс). */
    lv_obj_t *s3 = ui_modal_add_section(body, "Общее время работы");
    int total_pct = (int)((m->run_total_h * 100.0f) / m->run_max_h);
    ui_modal_state_t total_state =
        (total_pct >= 95) ? UI_MODAL_STATE_DANGER :
        (total_pct >= 80) ? UI_MODAL_STATE_WARN   : UI_MODAL_STATE_OK;

    format_hours(ph, sizeof(ph), m->run_total_h);
    format_hours(pm, sizeof(pm), m->run_max_h);
    snprintf(buf, sizeof(buf), "%s / %s", ph, pm);
    ui_modal_add_prop_row(s3, "Наработано", buf);
    ui_modal_add_progress_bar(s3, total_pct, total_state);

    float total_remaining = m->run_max_h - m->run_total_h;
    if (total_remaining < 0) total_remaining = 0;
    format_hours(buf, sizeof(buf), total_remaining);
    ui_modal_add_prop_row(s3, "Ресурс остался", buf);
    ui_modal_add_prop_row(s3, "Тип", m->type_name);
}

static const membrane_meta_t *find_membrane(const char *id)
{
    for (size_t i = 0; i < MEMBRANE_META_COUNT; i++) {
        if (strcmp(MEMBRANE_META[i].id, id) == 0) return &MEMBRANE_META[i];
    }
    return NULL;
}

/* ─── public dispatcher ───────────────────────────────────────────── */

void ui_equipment_modal_show(const char *equipment_id)
{
    if (!equipment_id) return;

    if (strcmp(equipment_id, "filter") == 0) {
        render_filter_modal();
        return;
    }
    const pump_meta_t *p = find_pump(equipment_id);
    if (p) { render_pump_modal(p); return; }

    const membrane_meta_t *m = find_membrane(equipment_id);
    if (m) { render_membrane_modal(m); return; }
}
