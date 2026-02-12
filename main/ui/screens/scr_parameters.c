/**
 * @file scr_parameters.c
 * @brief Экран "Параметры" -- таблица всех измеренных и расчётных величин.
 *
 * Прокручиваемый список с секциями: Давление, Температура, Расход,
 * Электропроводность, Расчётные показатели.
 * Каждая строка: Параметр | Значение | Доп.столбец | Статус.
 *
 * Данные: plant_data_t.pressure[], temperature, flow[], conductivity[], telemetry.
 * Область содержимого: 1280 x 700 px.
 */

#include "scr_parameters.h"
#include "ui_theme.h"
#include "ui_common.h"   // ui_fmt_float
#include "ui_fonts.h"
#include "lang.h"
#include <stdio.h>
#include <math.h>

/* ---- Количество строк в каждой секции ---- */
#define ROWS_PRESSURE     4  // P1..P4
#define ROWS_TEMPERATURE  1  // T
#define ROWS_FLOW         4  // Q1..Q4
#define ROWS_COND         3  // S1..S3
#define ROWS_CALC         6  // filter_dp, stage1_feed, recovery2, recovery_sys, sel1, sel2

/* ---- Хранилище виджетов ---- */

typedef struct {
    /* Давление -- значение и статус для каждого канала */
    lv_obj_t *lbl_p_val[ROWS_PRESSURE];
    lv_obj_t *lbl_p_status[ROWS_PRESSURE];

    /* Температура */
    lv_obj_t *lbl_t_val;
    lv_obj_t *lbl_t_status;

    /* Расход -- мгновенный, накопленный объём, статус */
    lv_obj_t *lbl_q_flow[ROWS_FLOW];    // м3/ч
    lv_obj_t *lbl_q_vol[ROWS_FLOW];     // м3 (накопительный)
    lv_obj_t *lbl_q_status[ROWS_FLOW];

    /* Электропроводность -- значение, температура компенсации, статус */
    lv_obj_t *lbl_c_val[ROWS_COND];     // мкСм/см
    lv_obj_t *lbl_c_temp[ROWS_COND];    // C (компенсация)
    lv_obj_t *lbl_c_status[ROWS_COND];

    /* Расчётные показатели */
    lv_obj_t *lbl_calc_val[ROWS_CALC];
} param_widgets_t;

/* ---- Вспомогательные функции ---- */

/** Создаёт заголовок секции (акцентный цвет, шрифт 18). */
static lv_obj_t *make_section_header(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_set_style_text_color(lbl, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_18, 0);
    lv_obj_set_style_pad_top(lbl, 10, 0);
    lv_obj_set_style_pad_bottom(lbl, 4, 0);
    return lbl;
}

/** Одна строка данных: имя | значение | доп.столбец (объём/температура) | статус. */
static void make_row(lv_obj_t *parent, const char *name,
                     lv_obj_t **val_out, lv_obj_t **extra_out,
                     lv_obj_t **status_out)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(row, 2, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Name (fixed width) */
    lv_obj_t *lbl_name = lv_label_create(row);
    lv_label_set_text(lbl_name, name);
    lv_obj_set_width(lbl_name, 140);
    lv_obj_set_style_text_color(lbl_name, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_name, UI_FONT_16, 0);

    /* Value */
    lv_obj_t *lbl_val = lv_label_create(row);
    lv_label_set_text(lbl_val, "---");
    lv_obj_set_width(lbl_val, 160);
    lv_obj_set_style_text_color(lbl_val, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(lbl_val, UI_FONT_16, 0);
    if (val_out) *val_out = lbl_val;

    /* Extra column (volume / temperature) */
    lv_obj_t *lbl_extra = lv_label_create(row);
    lv_label_set_text(lbl_extra, "");
    lv_obj_set_width(lbl_extra, 160);
    lv_obj_set_style_text_color(lbl_extra, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(lbl_extra, UI_FONT_16, 0);
    if (extra_out) *extra_out = lbl_extra;

    /* Status */
    lv_obj_t *lbl_st = lv_label_create(row);
    lv_label_set_text(lbl_st, "");
    lv_obj_set_width(lbl_st, 100);
    lv_obj_set_style_text_color(lbl_st, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl_st, UI_FONT_14, 0);
    if (status_out) *status_out = lbl_st;
}

/* ---- Создание экрана ---- */

/** Создаёт прокручиваемую таблицу параметров с заголовками столбцов и секциями. */
lv_obj_t *scr_parameters_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 1280, 700);
    lv_obj_set_style_bg_color(cont, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(cont, 16, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cont, 0, 0);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    param_widgets_t *w = lv_malloc(sizeof(param_widgets_t));
    lv_memzero(w, sizeof(param_widgets_t));

    /* ---- Column headers ---- */
    lv_obj_t *hdr_row = lv_obj_create(cont);
    lv_obj_remove_style_all(hdr_row);
    lv_obj_set_size(hdr_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(hdr_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(hdr_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(hdr_row, 8, 0);
    lv_obj_remove_flag(hdr_row, LV_OBJ_FLAG_SCROLLABLE);

    const char *col_names[] = {"Parameter", "Value", "Extra", "Status"};
    int col_widths[] = {140, 160, 160, 100};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *h = lv_label_create(hdr_row);
        lv_label_set_text(h, col_names[i]);
        lv_obj_set_width(h, col_widths[i]);
        lv_obj_set_style_text_color(h, COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(h, UI_FONT_14, 0);
    }

    /* ---- Pressure section ---- */
    make_section_header(cont, lang_str(STR_LBL_PRESSURE));
    for (int i = 0; i < ROWS_PRESSURE; i++) {
        char name[8];
        snprintf(name, sizeof(name), "P%d", i + 1);
        make_row(cont, name, &w->lbl_p_val[i], NULL, &w->lbl_p_status[i]);
    }

    /* ---- Temperature section ---- */
    make_section_header(cont, lang_str(STR_LBL_TEMPERATURE));
    make_row(cont, "T", &w->lbl_t_val, NULL, &w->lbl_t_status);

    /* ---- Flow section ---- */
    make_section_header(cont, lang_str(STR_LBL_FLOW_RATE));
    for (int i = 0; i < ROWS_FLOW; i++) {
        char name[8];
        snprintf(name, sizeof(name), "Q%d", i + 1);
        make_row(cont, name, &w->lbl_q_flow[i], &w->lbl_q_vol[i], &w->lbl_q_status[i]);
    }

    /* ---- Conductivity section ---- */
    make_section_header(cont, lang_str(STR_LBL_CONDUCTIVITY));
    for (int i = 0; i < ROWS_COND; i++) {
        char name[16];
        snprintf(name, sizeof(name), "S%d", i + 1);
        make_row(cont, name, &w->lbl_c_val[i], &w->lbl_c_temp[i], &w->lbl_c_status[i]);
    }

    /* ---- Calculated section ---- */
    make_section_header(cont, "Calculated");
    static const char *calc_names[] = {
        "Filter dP", "Stage1 Feed", "Recovery2", "Recovery sys", "Sel1", "Sel2"
    };
    for (int i = 0; i < ROWS_CALC; i++) {
        make_row(cont, calc_names[i], &w->lbl_calc_val[i], NULL, NULL);
    }

    lv_obj_set_user_data(cont, w);
    return cont;
}

/* ---- Обновление ---- */

/**
 * Обновляет все строки таблицы параметров.
 * Для каждого датчика: если fault -- показывает "---" и статус "Обрыв датчика";
 * иначе -- форматированное значение и статус "OK".
 */
void scr_parameters_update(lv_obj_t *container, const plant_data_t *d)
{
    param_widgets_t *w = (param_widgets_t *)lv_obj_get_user_data(container);
    if (!w) return;

    char buf[48];

    /* Pressure P1..P4 */
    for (int i = 0; i < ROWS_PRESSURE; i++) {
        if (d->pressure[i].fault) {
            lv_label_set_text(w->lbl_p_val[i], "---");
            lv_label_set_text(w->lbl_p_status[i], lang_str(STR_STATUS_SENSOR_BREAK));
            lv_obj_set_style_text_color(w->lbl_p_status[i], COLOR_PUMP_FAULT, 0);
        } else {
            ui_fmt_float(buf, sizeof(buf), d->pressure[i].value, 3,
                         lang_str(STR_UNIT_BAR));
            lv_label_set_text(w->lbl_p_val[i], buf);
            lv_label_set_text(w->lbl_p_status[i], lang_str(STR_STATUS_OK));
            lv_obj_set_style_text_color(w->lbl_p_status[i], COLOR_TEXT_SECONDARY, 0);
        }
    }

    /* Temperature */
    if (d->temperature.fault) {
        lv_label_set_text(w->lbl_t_val, "---");
        lv_label_set_text(w->lbl_t_status, lang_str(STR_STATUS_SENSOR_BREAK));
        lv_obj_set_style_text_color(w->lbl_t_status, COLOR_PUMP_FAULT, 0);
    } else {
        ui_fmt_float(buf, sizeof(buf), d->temperature.value, 1,
                     lang_str(STR_UNIT_CELSIUS));
        lv_label_set_text(w->lbl_t_val, buf);
        lv_label_set_text(w->lbl_t_status, lang_str(STR_STATUS_OK));
        lv_obj_set_style_text_color(w->lbl_t_status, COLOR_TEXT_SECONDARY, 0);
    }

    /* Flow Q1..Q4 */
    for (int i = 0; i < ROWS_FLOW; i++) {
        ui_fmt_float(buf, sizeof(buf), d->flow[i].flow, 3,
                     lang_str(STR_UNIT_M3H));
        lv_label_set_text(w->lbl_q_flow[i], buf);

        ui_fmt_float(buf, sizeof(buf), d->flow[i].volume, 2,
                     lang_str(STR_UNIT_M3));
        lv_label_set_text(w->lbl_q_vol[i], buf);

        if (d->flow[i].ok) {
            lv_label_set_text(w->lbl_q_status[i], lang_str(STR_STATUS_OK));
            lv_obj_set_style_text_color(w->lbl_q_status[i], COLOR_TEXT_SECONDARY, 0);
        } else {
            lv_label_set_text(w->lbl_q_status[i], lang_str(STR_STATUS_FAULT));
            lv_obj_set_style_text_color(w->lbl_q_status[i], COLOR_PUMP_FAULT, 0);
        }
    }

    /* Conductivity sigma1..sigma3 */
    for (int i = 0; i < ROWS_COND; i++) {
        ui_fmt_float(buf, sizeof(buf), d->conductivity[i].conductivity, 1,
                     lang_str(STR_UNIT_USCM));
        lv_label_set_text(w->lbl_c_val[i], buf);

        ui_fmt_float(buf, sizeof(buf), d->conductivity[i].temperature, 1,
                     lang_str(STR_UNIT_CELSIUS));
        lv_label_set_text(w->lbl_c_temp[i], buf);

        if (d->conductivity[i].ok) {
            lv_label_set_text(w->lbl_c_status[i], lang_str(STR_STATUS_OK));
            lv_obj_set_style_text_color(w->lbl_c_status[i], COLOR_TEXT_SECONDARY, 0);
        } else {
            lv_label_set_text(w->lbl_c_status[i], lang_str(STR_STATUS_FAULT));
            lv_obj_set_style_text_color(w->lbl_c_status[i], COLOR_PUMP_FAULT, 0);
        }
    }

    /* Calculated telemetry */
    ui_fmt_float(buf, sizeof(buf), d->telemetry.filter_dp, 3,
                 lang_str(STR_UNIT_BAR));
    lv_label_set_text(w->lbl_calc_val[0], buf);

    ui_fmt_float(buf, sizeof(buf), d->telemetry.stage1_feed, 2,
                 lang_str(STR_UNIT_M3H));
    lv_label_set_text(w->lbl_calc_val[1], buf);

    ui_fmt_float(buf, sizeof(buf), d->telemetry.recovery2, 1,
                 lang_str(STR_UNIT_PERCENT));
    lv_label_set_text(w->lbl_calc_val[2], buf);

    ui_fmt_float(buf, sizeof(buf), d->telemetry.recovery_sys, 1,
                 lang_str(STR_UNIT_PERCENT));
    lv_label_set_text(w->lbl_calc_val[3], buf);

    ui_fmt_float(buf, sizeof(buf), d->telemetry.sel1, 1,
                 lang_str(STR_UNIT_PERCENT));
    lv_label_set_text(w->lbl_calc_val[4], buf);

    ui_fmt_float(buf, sizeof(buf), d->telemetry.sel2, 1,
                 lang_str(STR_UNIT_PERCENT));
    lv_label_set_text(w->lbl_calc_val[5], buf);
}
