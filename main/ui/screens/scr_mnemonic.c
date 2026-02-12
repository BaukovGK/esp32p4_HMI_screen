/**
 * @file scr_mnemonic.c
 * @brief Мнемосхема -- главный экран P&ID-схемы процесса обратного осмоса.
 *
 * Технологический поток (слева направо):
 *   Исходная ёмкость -> Подающий насос -> Насос 1й ступени -> [Мембрана 1] ->
 *   Промежуточная ёмкость -> Насос 2й ступени -> [Мембрана 2] -> Ёмкость пермеата
 *
 * Экран содержит:
 *   - Три ёмкости с анимацией уровня (по дискретным входам DI)
 *   - Три насоса + нагреватель + дозатор (индикаторы-круги)
 *   - Две мембраны с трубопроводами концентрата/рецикла
 *   - Выноски датчиков: P1-P4 (давление), T (температура), Q1-Q4 (расход), σ1-σ3 (электропроводность)
 *   - Панель телеметрии (степень извлечения, перепад на фильтре)
 *   - Метка текущего состояния (IDLE/AUTO/WASHING/MANUAL/FAULT)
 *   - 5 кнопок управления: СТАРТ, СТОП, РУЧНОЙ, ПРОМЫВКА, СБРОС АВАРИИ
 *
 * Область содержимого: 1280 x 700 px.
 */

#include "scr_mnemonic.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_events.h"    // обработчики кнопок (MQTT-команды)
#include "ui_fonts.h"
#include "lang.h"         // мультиязычные строки
#include <stdio.h>
#include <math.h>

/* ---- Определения битов DI / DO ---- */

/* Дискретные входы (байт di) -- от модуля Waveshare AI */
#define DI_SOURCE_LOW (1u << 0)    /* DI1: нижний уровень исходной ёмкости  */
#define DI_SOURCE_HIGH (1u << 1)   /* DI2: верхний уровень исходной ёмкости */
#define DI_INTERM_LOW (1u << 2)    /* DI3: нижний уровень промежуточной     */
#define DI_INTERM_HIGH (1u << 3)   /* DI4: верхний уровень промежуточной    */
#define DI_PUMP1_RUN (1u << 4)     /* DI5: подтверждение работы насоса 1    */
#define DI_PUMP2_RUN (1u << 5)     /* DI6: подтверждение работы насоса 2    */
#define DI_PUMP3_RUN (1u << 6)     /* DI7: подтверждение работы насоса 3    */
#define DI_PERMEATE_HIGH (1u << 7) /* DI8: верхний уровень ёмкости пермеата */

/* Дискретные выходы (байт do_bits) */
#define DO_PUMP1 (1u << 0)   // Подающий насос
#define DO_PUMP2 (1u << 1)   // Насос 1й ступени
#define DO_PUMP3 (1u << 2)   // Насос 2й ступени
#define DO_HEATER (1u << 3)  // Нагреватель
#define DO_DOSER (1u << 4)   // Дозатор
#define DO_VALVE1 (1u << 5)  // Клапан 1
#define DO_VALVE2 (1u << 6)  // Клапан 2

/* ---- Константы разметки схемы ---- */

#define PIPE_Y 250   // Осевая линия горизонтальных трубопроводов (px)
#define TANK_TOP 115 // Верхний край прямоугольников ёмкостей
#define TANK_W 80    // Ширина ёмкости
#define TANK_H 220   // Высота ёмкости
#define PUMP_D 60    // Диаметр круга-индикатора насоса
#define MEMB_W 100   // Ширина блока мембраны
#define MEMB_H 80    // Высота блока мембраны

/* X-координаты элементов процесса (слева направо, заполняя 1280 px) */
#define X_SRC 15   // Исходная ёмкость
#define X_FEED 210 // Подающий насос
#define X_STG1 355 // Насос 1й ступени
#define X_M1 500   // Мембрана 1
#define X_INT 655  // Промежуточная ёмкость
#define X_STG2 850 // Насос 2й ступени
#define X_M2 995   // Мембрана 2
#define X_CLN 1150 // Ёмкость чистой воды (правый край ~1260)

/* Отрисовка трубопроводов */
#define PIPE_THICK 14                            // Толщина основного трубопровода
#define PIPE_COLOR       lv_color_hex(0x4A6A8A)  // Цвет основного трубопровода
#define PIPE_BORDER_CLR  lv_color_hex(0x3A5270)  // Цвет обводки трубопровода
#define PIPE_RECYCLE_THICK 8                     // Толщина линий концентрата/рецикла
#define PIPE_RECYCLE_CLR lv_color_hex(0x055BB0)  // Цвет линий рецикла

/* Выноска датчика (callout box) */
#define SBOX_H 30 // Высота выноски

/* ---- Хранилище виджетов (сохраняется в user_data корневого контейнера) ---- */

typedef struct
{
    /* Ёмкости: рамка + прямоугольник заполнения внутри */
    lv_obj_t *tank_source;       // Исходная ёмкость (рамка)
    lv_obj_t *tank_source_fill;  // Индикатор уровня исходной ёмкости
    lv_obj_t *tank_interm;       // Промежуточная ёмкость (рамка)
    lv_obj_t *tank_interm_fill;  // Индикатор уровня промежуточной
    lv_obj_t *tank_permeate;     // Ёмкость пермеата (рамка)
    lv_obj_t *tank_permeate_fill;// Индикатор уровня пермеата

    /* Индикаторы оборудования (круги) */
    lv_obj_t *ind_pump1;   // Подающий насос
    lv_obj_t *ind_pump2;   // Насос 1й ступени
    lv_obj_t *ind_pump3;   // Насос 2й ступени
    lv_obj_t *ind_heater;  // Нагреватель
    lv_obj_t *ind_doser;   // Дозатор

    /* Метки значений датчиков */
    lv_obj_t *lbl_p[4];   // Давление P1..P4 (bar)
    lv_obj_t *lbl_t;       // Температура T (C)
    lv_obj_t *lbl_q[4];   // Расход Q1..Q4 (m3/h)
    lv_obj_t *lbl_s[3];   // Электропроводность S1..S3 (uS/cm)

    /* Телеметрия */
    lv_obj_t *lbl_recovery;  // Степень извлечения системы (%)
    lv_obj_t *lbl_filter_dp; // Перепад давления на фильтре (bar)

    /* Метка текущего режима / состояния */
    lv_obj_t *lbl_state;

    /* Кнопки управления */
    lv_obj_t *btn_start;   // СТАРТ АВТО -> ui_evt_start_auto -> MQTT cmd/start_auto
    lv_obj_t *btn_stop;    // СТОП -> ui_evt_stop -> MQTT cmd/stop
    lv_obj_t *btn_manual;  // РУЧНОЙ -> ui_evt_set_manual -> MQTT cmd/manual
    lv_obj_t *btn_washing; // ПРОМЫВКА -> ui_evt_start_washing -> MQTT cmd/start_wash
    lv_obj_t *btn_reset;   // СБРОС АВАРИИ -> ui_evt_reset_fault -> MQTT cmd/reset
} mnemonic_widgets_t;

/* ---- Вспомогательные функции построения виджетов ---- */

/**
 * Создаёт виджет ёмкости: прямоугольная рамка + внутренний индикатор заполнения.
 * Уровень растёт снизу вверх. Подпись рисуется под ёмкостью.
 * @param fill_out  [out] указатель на объект-заполнитель для обновления высоты
 * @return рамка ёмкости (lv_obj)
 */
static lv_obj_t *make_tank(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t w, int32_t h, const char *title,
                           lv_obj_t **fill_out)
{
    /* Outer rectangle (border only) */
    lv_obj_t *frame = lv_obj_create(parent);
    lv_obj_remove_style_all(frame);
    lv_obj_set_size(frame, w, h);
    lv_obj_set_pos(frame, x, y);
    lv_obj_set_style_border_color(frame, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_border_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_20, 0);
    lv_obj_set_style_bg_color(frame, COLOR_BG_WIDGET, 0);
    lv_obj_set_style_radius(frame, 4, 0);
    lv_obj_set_style_pad_all(frame, 0, 0);
    lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    /* Fill indicator (grows from bottom) */
    lv_obj_t *fill = lv_obj_create(frame);
    lv_obj_remove_style_all(fill);
    lv_obj_set_size(fill, w - 4, 0);
    lv_obj_set_align(fill, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_color(fill, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_60, 0);
    lv_obj_set_style_radius(fill, 2, 0);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    *fill_out = fill;

    /* Title label below tank */
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
    lv_obj_set_pos(lbl, x, y + h + 4);

    return frame;
}

/**
 * Создаёт круглый индикатор оборудования (насос, нагреватель, дозатор).
 * Цвет фона меняется в scr_mnemonic_update в зависимости от DO/DI/fault.
 * Подпись рисуется под кругом.
 */
static lv_obj_t *make_indicator(lv_obj_t *parent, int32_t x, int32_t y,
                                int32_t diameter, const char *title)
{
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, diameter, diameter);
    lv_obj_set_pos(circle, x, y);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, COLOR_PUMP_OFF, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(circle, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_border_width(circle, 2, 0);
    lv_obj_remove_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    /* Title label below */
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_12, 0);
    lv_obj_set_pos(lbl, x, y + diameter + 2);

    return circle;
}

/** Горизонтальный основной трубопровод на осевой линии PIPE_Y. */
static void make_pipe(lv_obj_t *parent, int32_t x, int32_t length)
{
    lv_obj_t *pipe = lv_obj_create(parent);
    lv_obj_remove_style_all(pipe);
    lv_obj_set_size(pipe, length, PIPE_THICK);
    lv_obj_set_pos(pipe, x, PIPE_Y - PIPE_THICK / 2);
    lv_obj_set_style_bg_color(pipe, PIPE_COLOR, 0);
    lv_obj_set_style_bg_opa(pipe, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pipe, PIPE_BORDER_CLR, 0);
    lv_obj_set_style_border_width(pipe, 1, 0);
    lv_obj_set_style_radius(pipe, 3, 0);
    lv_obj_remove_flag(pipe, LV_OBJ_FLAG_SCROLLABLE);
}

/** Вертикальный основной трубопровод (cx -- центр по X). */
static void make_pipe_v(lv_obj_t *parent, int32_t cx, int32_t y,
                        int32_t height)
{
    lv_obj_t *pipe = lv_obj_create(parent);
    lv_obj_remove_style_all(pipe);
    lv_obj_set_size(pipe, PIPE_THICK, height);
    lv_obj_set_pos(pipe, cx - PIPE_THICK / 2, y);
    lv_obj_set_style_bg_color(pipe, PIPE_COLOR, 0);
    lv_obj_set_style_bg_opa(pipe, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pipe, PIPE_BORDER_CLR, 0);
    lv_obj_set_style_border_width(pipe, 1, 0);
    lv_obj_set_style_radius(pipe, 3, 0);
    lv_obj_remove_flag(pipe, LV_OBJ_FLAG_SCROLLABLE);
}

/** Тонкий трубопровод для линий концентрата / рецикла (горизонтальный или вертикальный). */
static void make_pipe_thin(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t w, int32_t h)
{
    lv_obj_t *pipe = lv_obj_create(parent);
    lv_obj_remove_style_all(pipe);
    lv_obj_set_size(pipe, w, h);
    lv_obj_set_pos(pipe, x, y);
    lv_obj_set_style_bg_color(pipe, PIPE_RECYCLE_CLR, 0);
    lv_obj_set_style_bg_opa(pipe, LV_OPA_70, 0);
    lv_obj_set_style_radius(pipe, 2, 0);
    lv_obj_remove_flag(pipe, LV_OBJ_FLAG_SCROLLABLE);
}

/** Создаёт блок мембраны (прямоугольник с названием внутри и подписью снизу). */
static void make_membrane(lv_obj_t *parent, int32_t x,
                          const char *short_name, const char *title)
{
    int32_t my = PIPE_Y - MEMB_H / 2;

    lv_obj_t *memb = lv_obj_create(parent);
    lv_obj_remove_style_all(memb);
    lv_obj_set_size(memb, MEMB_W, MEMB_H);
    lv_obj_set_pos(memb, x, my);
    lv_obj_set_style_bg_color(memb, lv_color_hex(0x1A3A5C), 0);
    lv_obj_set_style_bg_opa(memb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(memb, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(memb, 2, 0);
    lv_obj_set_style_radius(memb, 6, 0);
    lv_obj_remove_flag(memb, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(memb);
    lv_label_set_text(lbl, short_name);
    lv_obj_set_style_text_color(lbl, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_18, 0);
    lv_obj_center(lbl);

    lv_obj_t *tlbl = lv_label_create(parent);
    lv_label_set_text(tlbl, title);
    lv_obj_set_pos(tlbl, x, my + MEMB_H + 4);
    lv_obj_set_style_text_color(tlbl, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(tlbl, UI_FONT_12, 0);
}

/**
 * Создаёт выноску датчика -- прямоугольник с текстом и линией-выноской к трубопроводу.
 *
 *  bx, by   -- левый верхний угол рамки выноски
 *  pipe_x   -- координата X, где линия-выноска касается основного трубопровода
 *  text     -- начальный текст метки (например "P1: ---")
 *
 * Возвращает lv_label внутри рамки (для обновления значения).
 */
static lv_obj_t *make_sensor_box(lv_obj_t *parent, int32_t bx, int32_t by,
                                 int32_t pipe_x, const char *text)
{
    /* Bordered callout box */
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_SIZE_CONTENT, SBOX_H);
    lv_obj_set_pos(box, bx, by);
    lv_obj_set_style_bg_color(box, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_pad_hor(box, 8, 0);
    lv_obj_set_style_pad_ver(box, 5, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    /* Value label */
    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
    lv_obj_center(lbl);

    /* Leader line (thin vertical bar from box edge to pipe) */
    int32_t ly, lh;
    if (by + SBOX_H / 2 < PIPE_Y) {
        /* Box above pipe */
        ly = by + SBOX_H;
        lh = (PIPE_Y - PIPE_THICK / 2) - ly;
    } else {
        /* Box below pipe */
        ly = PIPE_Y + PIPE_THICK / 2;
        lh = by - ly;
    }
    if (lh > 0) {
        lv_obj_t *leader = lv_obj_create(parent);
        lv_obj_remove_style_all(leader);
        lv_obj_set_size(leader, 2, lh);
        lv_obj_set_pos(leader, pipe_x, ly);
        lv_obj_set_style_bg_color(leader, COLOR_ACCENT, 0);
        lv_obj_set_style_bg_opa(leader, LV_OPA_40, 0);
        lv_obj_remove_flag(leader, LV_OBJ_FLAG_SCROLLABLE);
        /* Push leader line behind all other objects */
        lv_obj_move_to_index(leader, 0);
    }

    return lbl;
}

/** Создаёт кнопку управления режимом (СТАРТ/СТОП/РУЧНОЙ/ПРОМЫВКА/СБРОС). */
static lv_obj_t *make_mode_btn(lv_obj_t *parent, int32_t x, int32_t y,
                               int32_t w, int32_t h,
                               const char *text, lv_color_t bg,
                               lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 6, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_16, 0);
    lv_obj_center(lbl);

    if (cb)
    {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    }
    return btn;
}

/* ---- Создание экрана ---- */

/** Создаёт полную мнемосхему: ёмкости, насосы, мембраны, трубопроводы, датчики, кнопки. */
lv_obj_t *scr_mnemonic_create(lv_obj_t *parent)
{
    /* Root container - fills content area */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, 1280, 700);
    lv_obj_set_style_bg_color(cont, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    mnemonic_widgets_t *w = lv_malloc(sizeof(mnemonic_widgets_t));
    lv_memzero(w, sizeof(mnemonic_widgets_t));

    /*
     * ============================================================
     *  PROCESS SCHEMA  (y = 15 .. 280)
     *
     *  [Source] → (Feed) → (Stage1) → |M1| → [Interm] → (Stage2) → |M2| → [Clean]
     * ============================================================
     */

    /* ---- Tanks ---- */
    w->tank_source = make_tank(cont, X_SRC, TANK_TOP, TANK_W, TANK_H,
                               lang_str(STR_LBL_SOURCE_TANK), &w->tank_source_fill);
    w->tank_interm = make_tank(cont, X_INT, TANK_TOP, TANK_W, TANK_H,
                               lang_str(STR_LBL_INTERM_TANK), &w->tank_interm_fill);
    w->tank_permeate = make_tank(cont, X_CLN, TANK_TOP, TANK_W, TANK_H,
                                 lang_str(STR_LBL_PERMEATE_TANK), &w->tank_permeate_fill);

    /* ---- Pumps ---- */
    w->ind_pump1 = make_indicator(cont, X_FEED, PIPE_Y - PUMP_D / 2, PUMP_D,
                                  lang_str(STR_LBL_FEED_PUMP));
    w->ind_pump2 = make_indicator(cont, X_STG1, PIPE_Y - PUMP_D / 2, PUMP_D,
                                  lang_str(STR_LBL_STAGE1_PUMP));
    w->ind_pump3 = make_indicator(cont, X_STG2, PIPE_Y - PUMP_D / 2, PUMP_D,
                                  lang_str(STR_LBL_STAGE2_PUMP));

    /* ---- Membranes ---- */
    make_membrane(cont, X_M1, "M1", lang_str(STR_LBL_MEMBRANE_1));
    make_membrane(cont, X_M2, "M2", lang_str(STR_LBL_MEMBRANE_2));

    /* ---- Heater & Doser (below pipe, near intermediate area) ---- */
    w->ind_heater = make_indicator(cont,  X_SRC + 110,
                                   PIPE_Y - 140, 50,
                                   lang_str(STR_LBL_HEATER));
    w->ind_doser = make_indicator(cont, X_FEED + PUMP_D +120,
                                  PIPE_Y - 180, 50,
                                  lang_str(STR_LBL_DOSER));

    /* ---- Main horizontal pipes (at PIPE_Y centerline) ---- */
    make_pipe(cont, X_SRC + TANK_W, X_FEED - (X_SRC + TANK_W));   /* Source → Feed   */
    make_pipe(cont, X_FEED + PUMP_D, X_STG1 - (X_FEED + PUMP_D)); /* Feed → Stage1   */
    make_pipe(cont, X_STG1 + PUMP_D, X_M1 - (X_STG1 + PUMP_D));   /* Stage1 → M1     */
    make_pipe(cont, X_M1 + MEMB_W, X_INT - (X_M1 + MEMB_W));      /* M1 → Interm     */
    make_pipe(cont, X_INT + TANK_W, X_STG2 - (X_INT + TANK_W));   /* Interm → Stage2 */
    make_pipe(cont, X_STG2 + PUMP_D, X_M2 - (X_STG2 + PUMP_D));   /* Stage2 → M2     */
    make_pipe(cont, X_M2 + MEMB_W, X_CLN - (X_M2 + MEMB_W));      /* M2 → Clean      */

    /*
     * ---- Concentrate / recycle pipes (thin, below main process) ----
     *
     * M1 concentrate → down → recycle Q2 back to before Stage1
     *                       → drain (right)
     * M2 concentrate → down → recycle Q4 back to Intermediate
     */
    {
        int32_t conc_y = PIPE_Y + MEMB_H / 2 + 15; /* just below membranes */
        int32_t m1_cx = X_M1 + MEMB_W / 2;         /* M1 center-x          */
        int32_t m2_cx = X_M2 + MEMB_W / 2;         /* M2 center-x          */
        int32_t rejoin_x = X_STG1 - 15;             /* recycle Q2 merge     */
        int32_t rejoin2_x = X_INT + TANK_W + 10;    /* recycle Q4 merge     */

        /* M1: vertical down from M1 bottom */
        make_pipe_thin(cont, m1_cx - PIPE_RECYCLE_THICK / 2,
                       PIPE_Y + MEMB_H / 2,
                       PIPE_RECYCLE_THICK, conc_y - (PIPE_Y + MEMB_H / 2));
        /* M1: horizontal left → recycle Q2 to before Stage1 */
        make_pipe_thin(cont, rejoin_x, conc_y,
                       m1_cx - rejoin_x, PIPE_RECYCLE_THICK);
        /* M1: vertical up at rejoin point to main pipe */
        make_pipe_thin(cont, rejoin_x - PIPE_RECYCLE_THICK / 2,
                       PIPE_Y + PIPE_THICK / 2,
                       PIPE_RECYCLE_THICK,
                       conc_y - (PIPE_Y + PIPE_THICK / 2));
        /* M1: horizontal right → drain stub */
        make_pipe_thin(cont, m1_cx, conc_y,
                       60, PIPE_RECYCLE_THICK);
        /* "Дренаж" label */
        lv_obj_t *lbl_drain = lv_label_create(cont);
        lv_label_set_text(lbl_drain, LV_SYMBOL_RIGHT " Дренаж");
        lv_obj_set_pos(lbl_drain, m1_cx + 72, conc_y + PIPE_RECYCLE_THICK +6);
        lv_obj_set_style_text_color(lbl_drain, PIPE_RECYCLE_CLR, 0);
        lv_obj_set_style_text_font(lbl_drain, UI_FONT_12, 0);

        /* M2: vertical down from M2 bottom */
        make_pipe_thin(cont, m2_cx - PIPE_RECYCLE_THICK / 2,
                       PIPE_Y + MEMB_H / 2,
                       PIPE_RECYCLE_THICK, conc_y - (PIPE_Y + MEMB_H / 2));
        /* M2: horizontal left → recycle Q4 to Intermediate */
        make_pipe_thin(cont, rejoin2_x, conc_y,
                       m2_cx - rejoin2_x, PIPE_RECYCLE_THICK);
        /* M2: vertical up at rejoin point to main pipe */
        make_pipe_thin(cont, rejoin2_x - PIPE_RECYCLE_THICK / 2,
                       PIPE_Y + PIPE_THICK / 2,
                       PIPE_RECYCLE_THICK,
                       conc_y - (PIPE_Y + PIPE_THICK / 2));

        /* Recycle labels */
        lv_obj_t *lbl_q2 = lv_label_create(cont);
        lv_label_set_text(lbl_q2, "Q2 рецикл");
        lv_obj_set_pos(lbl_q2, rejoin_x + 20, conc_y + PIPE_RECYCLE_THICK + 2);
        lv_obj_set_style_text_color(lbl_q2, PIPE_RECYCLE_CLR, 0);
        lv_obj_set_style_text_font(lbl_q2, UI_FONT_12, 0);

        lv_obj_t *lbl_q4r = lv_label_create(cont);
        lv_label_set_text(lbl_q4r, "Q4 рецикл");
        lv_obj_set_pos(lbl_q4r, rejoin2_x + 20, conc_y + PIPE_RECYCLE_THICK + 2);
        lv_obj_set_style_text_color(lbl_q4r, PIPE_RECYCLE_CLR, 0);
        lv_obj_set_style_text_font(lbl_q4r, UI_FONT_12, 0);
    }

    /*
     * ============================================================
     *  SENSOR CALLOUT BOXES
     *
     *  Above pipe (y=20):  P1, P2, P3, P4
     *  Below pipe (y=365): T, Q1, Q2, Q3, Q4
     *  Below pipe (y=400): σ1, σ2, σ3
     * ============================================================
     */

    /* Pressure P1-P4 (above pipe, y=20) */
    w->lbl_p[0] = make_sensor_box(cont, 195, 20, 240, "P1: ---");
    w->lbl_p[1] = make_sensor_box(cont, 310, 20, 340, "P2: ---");
    w->lbl_p[2] = make_sensor_box(cont, 430, 20, 460, "P3: ---");
    w->lbl_p[3] = make_sensor_box(cont, 920, 20, 950, "P4: ---");

    /* Temperature (below pipe) */
    w->lbl_t = make_sensor_box(cont, 30, 365, 150, "T: ---");

    /* Flow Q1-Q4 (below pipe, y=365) */
    w->lbl_q[0] = make_sensor_box(cont, 195, 365, 240, "Q1: ---");
    w->lbl_q[1] = make_sensor_box(cont, 475, 365, 555, "Q2: ---");
    w->lbl_q[2] = make_sensor_box(cont, 1060, 365, 1120, "Q3: ---");
    w->lbl_q[3] = make_sensor_box(cont, 920, 365, 975, "Q4: ---");

    /* Conductivity σ1-σ3 (below pipe, y=400) */
    w->lbl_s[0] = make_sensor_box(cont, 195, 400, 240, "σ1: ---");
    w->lbl_s[1] = make_sensor_box(cont, 580, 400, 630, "σ2: ---");
    w->lbl_s[2] = make_sensor_box(cont, 1060, 400, 1120, "σ3: ---");

    /*
     * ============================================================
     *  TELEMETRY  (y = 480)
     * ============================================================
     */

    lv_obj_t *tel_panel = lv_obj_create(cont);
    lv_obj_remove_style_all(tel_panel);
    lv_obj_set_size(tel_panel, 620, 80);
    lv_obj_set_pos(tel_panel, 30, 480);
    lv_obj_set_style_bg_color(tel_panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(tel_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tel_panel, 8, 0);
    lv_obj_set_style_pad_all(tel_panel, 12, 0);
    lv_obj_remove_flag(tel_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Recovery */
    lv_obj_t *rec_title = lv_label_create(tel_panel);
    lv_label_set_text(rec_title, lang_str(STR_LBL_RECOVERY));
    lv_obj_set_style_text_color(rec_title, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(rec_title, UI_FONT_14, 0);
    lv_obj_set_pos(rec_title, 0, 0);

    w->lbl_recovery = lv_label_create(tel_panel);
    lv_label_set_text(w->lbl_recovery, "--- %");
    lv_obj_set_style_text_color(w->lbl_recovery, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(w->lbl_recovery, UI_FONT_28, 0);
    lv_obj_set_pos(w->lbl_recovery, 0, 22);

    /* Filter DP */
    lv_obj_t *dp_title = lv_label_create(tel_panel);
    lv_label_set_text(dp_title, lang_str(STR_LBL_FILTER_DP));
    lv_obj_set_style_text_color(dp_title, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(dp_title, UI_FONT_14, 0);
    lv_obj_set_pos(dp_title, 310, 0);

    w->lbl_filter_dp = lv_label_create(tel_panel);
    lv_label_set_text(w->lbl_filter_dp, "--- bar");
    lv_obj_set_style_text_color(w->lbl_filter_dp, COLOR_TEXT_VALUE, 0);
    lv_obj_set_style_text_font(w->lbl_filter_dp, UI_FONT_22, 0);
    lv_obj_set_pos(w->lbl_filter_dp, 310, 22);

    /* ---- State label (right of telemetry panel) ---- */
    w->lbl_state = lv_label_create(cont);
    lv_label_set_text(w->lbl_state, "---");
    lv_obj_set_pos(w->lbl_state, 700, 500);
    lv_obj_set_style_text_color(w->lbl_state, COLOR_STATE_IDLE, 0);
    lv_obj_set_style_text_font(w->lbl_state, UI_FONT_28, 0);

    /*
     * ============================================================
     *  CONTROL BUTTONS  (full width, centered, 50px from bottom)
     * ============================================================
     */

    int32_t btn_h = 54;
    int32_t btn_gap = 16;
    int32_t btn_w = 230;
    /* 5 buttons: 5*230 + 4*16 = 1214, margin = (1280-1214)/2 = 33 */
    int32_t btn_x0 = 33;
    int32_t btn_y = 700 - 50 - btn_h; /* 50px from bottom edge */

    w->btn_start = make_mode_btn(cont, btn_x0, btn_y, btn_w, btn_h,
                                 lang_str(STR_BTN_START_AUTO),
                                 COLOR_BTN_SUCCESS, ui_evt_start_auto);

    w->btn_stop = make_mode_btn(cont, btn_x0 + (btn_w + btn_gap), btn_y, btn_w, btn_h,
                                lang_str(STR_BTN_STOP),
                                COLOR_BTN_DANGER, ui_evt_stop);

    w->btn_manual = make_mode_btn(cont, btn_x0 + 2 * (btn_w + btn_gap), btn_y, btn_w, btn_h,
                                  lang_str(STR_BTN_MANUAL),
                                  COLOR_STATE_MANUAL, ui_evt_set_manual);

    w->btn_washing = make_mode_btn(cont, btn_x0 + 3 * (btn_w + btn_gap), btn_y, btn_w, btn_h,
                                   lang_str(STR_BTN_START_WASHING),
                                   COLOR_STATE_WASHING, ui_evt_start_washing);

    w->btn_reset = make_mode_btn(cont, btn_x0 + 4 * (btn_w + btn_gap), btn_y, btn_w, btn_h,
                                 lang_str(STR_BTN_RESET_FAULT),
                                 COLOR_ALARM_ALARM, ui_evt_reset_fault);
    lv_obj_add_flag(w->btn_reset, LV_OBJ_FLAG_HIDDEN);

    /* Store widget pointers */
    lv_obj_set_user_data(cont, w);

    return cont;
}

/* ---- Обновление виджетов ---- */

/**
 * Устанавливает цвет индикатора оборудования по логике:
 *   fault           -> красный (FAULT)
 *   DO=1, DI=1      -> зелёный (работает)
 *   DO=1, DI=0      -> жёлтый  (запускается, нет подтверждения)
 *   DO=0            -> серый   (выключен)
 */
static void set_indicator_color(lv_obj_t *ind, bool do_on, bool di_confirm, bool fault)
{
    lv_color_t c;
    if (fault)
    {
        c = COLOR_PUMP_FAULT;
    }
    else if (do_on && di_confirm)
    {
        c = COLOR_PUMP_RUNNING;
    }
    else if (do_on && !di_confirm)
    {
        c = COLOR_PUMP_STARTING;
    }
    else
    {
        c = COLOR_PUMP_OFF;
    }
    lv_obj_set_style_bg_color(ind, c, 0);
}

/**
 * Устанавливает высоту индикатора заполнения ёмкости по дискретным датчикам уровня.
 * low=0, high=0 -> пусто (5%); low=1, high=0 -> средний (50%); low=1, high=1 -> полная (100%)
 */
static void set_tank_fill(lv_obj_t *fill, lv_obj_t *frame, bool low, bool high)
{
    int pct = 0;
    if (low && high)
        pct = 100;
    else if (low && !high)
        pct = 50;
    else
        pct = 5; /* minimum visible sliver */

    int32_t tank_h = lv_obj_get_height(frame) - 4;
    int32_t fill_h = (tank_h * pct) / 100;
    lv_obj_set_height(fill, fill_h);
}

/**
 * Обновляет все виджеты мнемосхемы по данным plant_data_t.
 * Вызывается периодически (~500 мс) из UI-таймера.
 * Читает: di, do_bits, pressure[], temperature, flow[], conductivity[],
 *          telemetry, doser, state.
 */
void scr_mnemonic_update(lv_obj_t *container, const plant_data_t *d)
{
    mnemonic_widgets_t *w = (mnemonic_widgets_t *)lv_obj_get_user_data(container);
    if (!w)
        return;

    char buf[48];
    uint8_t di = d->di;
    uint8_t do_bits = d->do_bits;

    /* ---- Tank fill levels ---- */
    set_tank_fill(w->tank_source_fill, w->tank_source,
                  !(di & DI_SOURCE_LOW), (di & DI_SOURCE_HIGH));
    set_tank_fill(w->tank_interm_fill, w->tank_interm,
                  !(di & DI_INTERM_LOW), (di & DI_INTERM_HIGH));
    /* Permeate: only high-level sensor available */
    set_tank_fill(w->tank_permeate_fill, w->tank_permeate,
                  true, (di & DI_PERMEATE_HIGH));

    /* ---- Equipment indicators ---- */
    set_indicator_color(w->ind_pump1, do_bits & DO_PUMP1, di & DI_PUMP1_RUN,
                        d->state == PLANT_STATE_FAULT);
    set_indicator_color(w->ind_pump2, do_bits & DO_PUMP2, di & DI_PUMP2_RUN,
                        d->state == PLANT_STATE_FAULT);
    set_indicator_color(w->ind_pump3, do_bits & DO_PUMP3, di & DI_PUMP3_RUN,
                        d->state == PLANT_STATE_FAULT);

    /* Heater: no DI confirm, just DO state */
    lv_obj_set_style_bg_color(w->ind_heater,
                              (do_bits & DO_HEATER) ? COLOR_PUMP_RUNNING : COLOR_PUMP_OFF, 0);

    /* Doser */
    lv_color_t doser_c = COLOR_PUMP_OFF;
    if (d->doser.state == DOSER_STATE_RUNNING)
        doser_c = COLOR_PUMP_RUNNING;
    else if (d->doser.state == DOSER_STATE_PAUSE)
        doser_c = COLOR_PUMP_STARTING;
    lv_obj_set_style_bg_color(w->ind_doser, doser_c, 0);

    /* ---- Pressure labels P1..P4 ---- */
    for (int i = 0; i < 4; i++)
    {
        if (d->pressure[i].fault)
        {
            snprintf(buf, sizeof(buf), "P%d: FAULT", i + 1);
            lv_label_set_text(w->lbl_p[i], buf);
            lv_obj_set_style_text_color(w->lbl_p[i], COLOR_PUMP_FAULT, 0);
        }
        else
        {
            ui_fmt_float(buf, sizeof(buf), d->pressure[i].value, 2,
                         lang_str(STR_UNIT_BAR));
            char full[64];
            snprintf(full, sizeof(full), "P%d: %s", i + 1, buf);
            lv_label_set_text(w->lbl_p[i], full);
            lv_obj_set_style_text_color(w->lbl_p[i], COLOR_TEXT_VALUE, 0);
        }
    }

    /* Temperature */
    if (d->temperature.fault)
    {
        lv_label_set_text(w->lbl_t, "T: FAULT");
        lv_obj_set_style_text_color(w->lbl_t, COLOR_PUMP_FAULT, 0);
    }
    else
    {
        ui_fmt_float(buf, sizeof(buf), d->temperature.value, 1,
                     lang_str(STR_UNIT_CELSIUS));
        char full[64];
        snprintf(full, sizeof(full), "T: %s", buf);
        lv_label_set_text(w->lbl_t, full);
        lv_obj_set_style_text_color(w->lbl_t, COLOR_TEXT_VALUE, 0);
    }

    /* Flow Q1..Q4 */
    for (int i = 0; i < 4; i++)
    {
        ui_fmt_float(buf, sizeof(buf), d->flow[i].flow, 2,
                     lang_str(STR_UNIT_M3H));
        char full[64];
        snprintf(full, sizeof(full), "Q%d: %s", i + 1, buf);
        lv_label_set_text(w->lbl_q[i], full);
        lv_obj_set_style_text_color(w->lbl_q[i],
                                    d->flow[i].ok ? COLOR_TEXT_VALUE : COLOR_PUMP_FAULT, 0);
    }

    /* Conductivity S1..S3 */
    for (int i = 0; i < 3; i++)
    {
        ui_fmt_float(buf, sizeof(buf), d->conductivity[i].conductivity, 1,
                     lang_str(STR_UNIT_USCM));
        char full[64];
        snprintf(full, sizeof(full), "σ%d: %s", i + 1, buf);
        lv_label_set_text(w->lbl_s[i], full);
        lv_obj_set_style_text_color(w->lbl_s[i],
                                    d->conductivity[i].ok ? COLOR_TEXT_VALUE : COLOR_PUMP_FAULT, 0);
    }

    /* ---- Telemetry ---- */
    ui_fmt_float(buf, sizeof(buf), d->telemetry.recovery_sys, 1,
                 lang_str(STR_UNIT_PERCENT));
    lv_label_set_text(w->lbl_recovery, buf);

    ui_fmt_float(buf, sizeof(buf), d->telemetry.filter_dp, 3,
                 lang_str(STR_UNIT_BAR));
    lv_label_set_text(w->lbl_filter_dp, buf);

    /* ---- State label ---- */
    const char *state_text;
    lv_color_t state_color;
    switch (d->state)
    {
    case PLANT_STATE_IDLE:
        state_text = lang_str(STR_MODE_IDLE);
        state_color = COLOR_STATE_IDLE;
        break;
    case PLANT_STATE_AUTO:
        state_text = lang_str(STR_MODE_AUTO);
        state_color = COLOR_STATE_AUTO;
        break;
    case PLANT_STATE_WASHING:
        state_text = lang_str(STR_MODE_WASHING);
        state_color = COLOR_STATE_WASHING;
        break;
    case PLANT_STATE_MANUAL:
        state_text = lang_str(STR_MODE_MANUAL);
        state_color = COLOR_STATE_MANUAL;
        break;
    case PLANT_STATE_FAULT:
        state_text = lang_str(STR_MODE_FAULT);
        state_color = COLOR_STATE_FAULT;
        break;
    default:
        state_text = lang_str(STR_MODE_UNKNOWN);
        state_color = COLOR_STATE_IDLE;
        break;
    }
    lv_label_set_text(w->lbl_state, state_text);
    lv_obj_set_style_text_color(w->lbl_state, state_color, 0);

    /* ---- RESET button visibility ---- */
    if (d->state == PLANT_STATE_FAULT)
    {
        lv_obj_remove_flag(w->btn_reset, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(w->btn_reset, LV_OBJ_FLAG_HIDDEN);
    }
}
