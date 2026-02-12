/**
 * @file scr_diagnostics.c
 * @brief Экран "Диагностика" -- системная информация контроллера ESP32-S3.
 *
 * Три панели:
 *   1. Система (вверху-слева): свободная heap, минимум heap, uptime, статус MQTT
 *   2. Modbus (вверху-справа): таблица 4 устройств (Waveshare AI, URZh2KM, SL21-201, SL21-101)
 *      с колонками: имя, статус (Online/Offline), счётчик ошибок
 *   3. Стеки задач (внизу): таблица 5 задач (Modbus, IO, Process, Watchdog, MQTT)
 *      с колонкой свободного места стека в байтах
 *
 * Цветовая индикация:
 *   - Heap < 32 KB -> красный; heap_min < 16 KB -> красный
 *   - Стек < 512 B -> красный
 *   - Modbus ошибки > 0 -> жёлтый
 *   - MQTT offline -> красный
 *
 * Данные: plant_data_t.diagnostics, plant_data_t.mqtt_connected.
 * Область содержимого: 1280 x 700 px.
 */

#include "scr_diagnostics.h"
#include "ui_theme.h"
#include "ui_common.h"   // ui_fmt_uptime
#include "ui_fonts.h"
#include "lang.h"
#include <stdio.h>
#include <inttypes.h>

/* ---- Имена Modbus-устройств ---- */

#define MODBUS_DEV_COUNT 4

static const char *modbus_dev_names[MODBUS_DEV_COUNT] = {
    "Waveshare AI",  // Модуль аналоговых входов (4-20 мА)
    "URZh2KM",       // Счётчик расхода
    "SL21-201",      // Кондуктометр 1
    "SL21-101",      // Кондуктометр 2
};

/* ---- Имена задач RTOS ---- */

#define TASK_COUNT 5

static const char *task_names[TASK_COUNT] = {
    "Modbus",    // Задача опроса Modbus
    "IO",        // Задача обработки дискретных I/O
    "Process",   // Основная задача технологического процесса
    "Watchdog",  // Задача сторожевого таймера
    "MQTT",      // Задача MQTT-клиента
};

/* ---- Хранилище виджетов ---- */

typedef struct {
    /* Системная информация */
    lv_obj_t *lbl_heap_free;   // Свободная heap (байт)
    lv_obj_t *lbl_heap_min;    // Минимум heap за всё время (байт)
    lv_obj_t *lbl_uptime;      // Время работы (dd:hh:mm:ss)
    lv_obj_t *lbl_mqtt;        // Статус MQTT (Online/Offline)

    /* Modbus: статус и счётчик ошибок для каждого устройства */
    lv_obj_t *lbl_mb_online[MODBUS_DEV_COUNT];
    lv_obj_t *lbl_mb_errors[MODBUS_DEV_COUNT];

    /* Стеки задач: свободное место */
    lv_obj_t *lbl_stack[TASK_COUNT];
} diag_widgets_t;

/* ---- Вспомогательные функции ---- */

/** Создаёт информационную панель с заголовком (скруглённый прямоугольник). */
static lv_obj_t *make_info_panel(lv_obj_t *parent, int32_t x, int32_t y,
                                 int32_t w, int32_t h, const char *title)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_style_bg_color(panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 16, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_pos(lbl_title, 0, 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(lbl_title, UI_FONT_16, 0);

    return panel;
}

/** Создаёт строку "ключ: значение" внутри панели. Возвращает lv_label значения. */
static lv_obj_t *make_kv_row(lv_obj_t *panel, int32_t y_offset,
                             const char *key, const char *initial_val)
{
    lv_obj_t *k = lv_label_create(panel);
    lv_label_set_text(k, key);
    lv_obj_set_pos(k, 0, y_offset);
    lv_obj_set_style_text_color(k, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(k, UI_FONT_14, 0);

    lv_obj_t *v = lv_label_create(panel);
    lv_label_set_text(v, initial_val);
    lv_obj_set_pos(v, 200, y_offset);
    lv_obj_set_style_text_color(v, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(v, UI_FONT_14, 0);

    return v;
}

/* ---- Создание экрана ---- */

/** Создаёт экран диагностики: панели системы, Modbus и стеков задач. */
lv_obj_t *scr_diagnostics_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 1280, 700);
    lv_obj_set_style_bg_color(cont, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    diag_widgets_t *w = lv_malloc(sizeof(diag_widgets_t));
    lv_memzero(w, sizeof(diag_widgets_t));

    /* ---- System panel (top-left) ---- */
    lv_obj_t *sys = make_info_panel(cont, 20, 16, 380, 200, "System");

    w->lbl_heap_free = make_kv_row(sys, 30, lang_str(STR_DIAG_HEAP_FREE), "---");
    w->lbl_heap_min  = make_kv_row(sys, 56, lang_str(STR_DIAG_HEAP_MIN),  "---");
    w->lbl_uptime    = make_kv_row(sys, 82, lang_str(STR_DIAG_UPTIME),    "---");
    w->lbl_mqtt      = make_kv_row(sys, 108, lang_str(STR_DIAG_MQTT_STATUS), "---");

    /* ---- Modbus panel (top-right) ---- */
    lv_obj_t *mb = make_info_panel(cont, 420, 16, 840, 200, lang_str(STR_DIAG_MODBUS));

    /* Table header */
    lv_obj_t *hdr_name = lv_label_create(mb);
    lv_label_set_text(hdr_name, "Device");
    lv_obj_set_pos(hdr_name, 0, 30);
    lv_obj_set_style_text_color(hdr_name, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(hdr_name, UI_FONT_14, 0);

    lv_obj_t *hdr_status = lv_label_create(mb);
    lv_label_set_text(hdr_status, "Status");
    lv_obj_set_pos(hdr_status, 200, 30);
    lv_obj_set_style_text_color(hdr_status, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(hdr_status, UI_FONT_14, 0);

    lv_obj_t *hdr_err = lv_label_create(mb);
    lv_label_set_text(hdr_err, "Errors");
    lv_obj_set_pos(hdr_err, 350, 30);
    lv_obj_set_style_text_color(hdr_err, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(hdr_err, UI_FONT_14, 0);

    for (int i = 0; i < MODBUS_DEV_COUNT; i++) {
        int32_t ry = 56 + i * 28;

        lv_obj_t *nm = lv_label_create(mb);
        lv_label_set_text(nm, modbus_dev_names[i]);
        lv_obj_set_pos(nm, 0, ry);
        lv_obj_set_style_text_color(nm, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(nm, UI_FONT_14, 0);

        w->lbl_mb_online[i] = lv_label_create(mb);
        lv_label_set_text(w->lbl_mb_online[i], "---");
        lv_obj_set_pos(w->lbl_mb_online[i], 200, ry);
        lv_obj_set_style_text_font(w->lbl_mb_online[i], UI_FONT_14, 0);

        w->lbl_mb_errors[i] = lv_label_create(mb);
        lv_label_set_text(w->lbl_mb_errors[i], "---");
        lv_obj_set_pos(w->lbl_mb_errors[i], 350, ry);
        lv_obj_set_style_text_color(w->lbl_mb_errors[i], COLOR_TEXT_VALUE, 0);
        lv_obj_set_style_text_font(w->lbl_mb_errors[i], UI_FONT_14, 0);
    }

    /* ---- Task stacks panel (bottom) ---- */
    lv_obj_t *stk = make_info_panel(cont, 20, 236, 1240, 240, "Task Stack Watermarks");

    /* Table header */
    lv_obj_t *shdr_name = lv_label_create(stk);
    lv_label_set_text(shdr_name, "Task");
    lv_obj_set_pos(shdr_name, 0, 30);
    lv_obj_set_style_text_color(shdr_name, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(shdr_name, UI_FONT_14, 0);

    lv_obj_t *shdr_val = lv_label_create(stk);
    lv_label_set_text(shdr_val, "Free (bytes)");
    lv_obj_set_pos(shdr_val, 200, 30);
    lv_obj_set_style_text_color(shdr_val, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(shdr_val, UI_FONT_14, 0);

    for (int i = 0; i < TASK_COUNT; i++) {
        int32_t ry = 56 + i * 26;

        lv_obj_t *tn = lv_label_create(stk);
        lv_label_set_text(tn, task_names[i]);
        lv_obj_set_pos(tn, 0, ry);
        lv_obj_set_style_text_color(tn, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(tn, UI_FONT_14, 0);

        w->lbl_stack[i] = lv_label_create(stk);
        lv_label_set_text(w->lbl_stack[i], "---");
        lv_obj_set_pos(w->lbl_stack[i], 200, ry);
        lv_obj_set_style_text_color(w->lbl_stack[i], COLOR_TEXT_VALUE, 0);
        lv_obj_set_style_text_font(w->lbl_stack[i], UI_FONT_14, 0);
    }

    lv_obj_set_user_data(cont, w);
    return cont;
}

/* ---- Обновление ---- */

/**
 * Обновляет все диагностические показатели.
 * Читает: diagnostics (heap, uptime, modbus_online/errors, stack_*), mqtt_connected.
 * Критичные значения подсвечиваются красным (heap < 32KB, стек < 512B, modbus offline).
 */
void scr_diagnostics_update(lv_obj_t *container, const plant_data_t *d)
{
    diag_widgets_t *w = (diag_widgets_t *)lv_obj_get_user_data(container);
    if (!w) return;

    char buf[64];
    const diagnostics_t *diag = &d->diagnostics;

    /* Heap */
    snprintf(buf, sizeof(buf), "%"PRIu32" bytes", diag->heap_free);
    lv_label_set_text(w->lbl_heap_free, buf);

    snprintf(buf, sizeof(buf), "%"PRIu32" bytes", diag->heap_min);
    lv_label_set_text(w->lbl_heap_min, buf);

    /* Color heap red if critically low (<32 KB) */
    lv_obj_set_style_text_color(w->lbl_heap_free,
        (diag->heap_free < 32768) ? COLOR_PUMP_FAULT : COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_color(w->lbl_heap_min,
        (diag->heap_min < 16384) ? COLOR_PUMP_FAULT : COLOR_TEXT_VALUE, 0);

    /* Uptime */
    ui_fmt_uptime(buf, sizeof(buf), diag->uptime_s);
    lv_label_set_text(w->lbl_uptime, buf);

    /* MQTT status */
    if (d->mqtt_connected) {
        lv_label_set_text(w->lbl_mqtt, lang_str(STR_STATUS_ONLINE));
        lv_obj_set_style_text_color(w->lbl_mqtt, COLOR_PUMP_RUNNING, 0);
    } else {
        lv_label_set_text(w->lbl_mqtt, lang_str(STR_STATUS_OFFLINE));
        lv_obj_set_style_text_color(w->lbl_mqtt, COLOR_PUMP_FAULT, 0);
    }

    /* Modbus devices */
    for (int i = 0; i < MODBUS_DEV_COUNT; i++) {
        if (diag->modbus_online[i]) {
            lv_label_set_text(w->lbl_mb_online[i], lang_str(STR_STATUS_ONLINE));
            lv_obj_set_style_text_color(w->lbl_mb_online[i], COLOR_PUMP_RUNNING, 0);
        } else {
            lv_label_set_text(w->lbl_mb_online[i], lang_str(STR_STATUS_OFFLINE));
            lv_obj_set_style_text_color(w->lbl_mb_online[i], COLOR_PUMP_FAULT, 0);
        }

        snprintf(buf, sizeof(buf), "%"PRIu32, diag->modbus_errors[i]);
        lv_label_set_text(w->lbl_mb_errors[i], buf);

        /* Color error count if > 0 */
        lv_obj_set_style_text_color(w->lbl_mb_errors[i],
            (diag->modbus_errors[i] > 0) ? COLOR_ALARM_WARNING : COLOR_TEXT_VALUE, 0);
    }

    /* Task stacks */
    const uint32_t *stacks[TASK_COUNT] = {
        &diag->stack_modbus,
        &diag->stack_io,
        &diag->stack_process,
        &diag->stack_watchdog,
        &diag->stack_mqtt,
    };

    for (int i = 0; i < TASK_COUNT; i++) {
        snprintf(buf, sizeof(buf), "%"PRIu32" bytes", *stacks[i]);
        lv_label_set_text(w->lbl_stack[i], buf);

        /* Color red if stack low (<512 bytes) */
        lv_obj_set_style_text_color(w->lbl_stack[i],
            (*stacks[i] < 512) ? COLOR_PUMP_FAULT : COLOR_TEXT_VALUE, 0);
    }
}
