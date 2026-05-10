/**
 * @file scr_mnemonic.c
 * @brief Главный экран — мнемосхема установки RO.
 *
 * Полностью переработан на базе новых виджетов (фаза 2 LVGL_PLAN):
 *   ui_tank, ui_pump, ui_filter, ui_membrane, ui_sensor, ui_pipe.
 *
 * Координаты — прямой порт из proto/index.html (SVG viewBox 0 30 900 530).
 * Конвертация в физические пиксели через MNEMO_PX(svg). Y-координаты SVG
 * сдвигаются на -30 (учёт viewBox y-offset), см. SY ниже.
 *
 * Layout:
 *   ROW 1 (svg y=210): Исходная → Преднагн → ◇Фильтр → 1-я ст → Мембр.1 → дренаж
 *   ROW 2 (svg y=350): Чистая ← Мембр.2 ← 2-я ст ← Промеж
 *   Recycle 1 — концентрат мембр.1 (top-center) → перед 1-й ст
 *   Recycle 2 — концентрат мембр.2 (bottom-center) → перед 2-й ст
 *   Дозатор реагента → точка врезки (435, 210) ПОСЛЕ фильтра
 *
 * Widgets-context (mnemonic_widgets_t) хранится в user_data root'а
 * для последующих updates из scr_mnemonic_update().
 */
#include "scr_mnemonic.h"
#include "ui_tokens.h"
#include "ui_fonts.h"
#include "ui_tank.h"
#include "ui_pump.h"
#include "ui_filter.h"
#include "ui_membrane.h"
#include "ui_sensor.h"
#include "ui_pipe.h"
#include "ui_sensor_modal.h"
#include "ui_equipment_modal.h"
#include "ui_events.h"
#include "lang.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ─── coord helpers ─────────────────────────────────────────────────
 * SX / SY — конвертеры SVG-координат прототипа в физические пиксели.
 * Прототип использует viewBox "0 30 900 530" — вычитаем 30 из y. */
#define SX(svg_x) MNEMO_PX(svg_x)
#define SY(svg_y) MNEMO_PX((svg_y) - 30)

/* ─── layout главного экрана ───────────────────────────────────────
 * Сетка: [мнемо] [right panel 320] с padding 12 и gap 12.
 *   inner_w = 1280 - 24 = 1256
 *   left_area_w = 1256 - 12 - 320 = 924
 *   left_area_h = 692 - 24 = 668
 *   canvas (MNEMO_PX_W × MNEMO_PX_H = 900×530) центрируется в left area. */
#define PAGE_PAD       12
#define COL_GAP        12
#define RIGHT_PANEL_W  320
#define LEFT_AREA_W    (UI_SCREEN_W - 2 * PAGE_PAD - COL_GAP - RIGHT_PANEL_W)
#define LEFT_AREA_H    (UI_CONTENT_H - 2 * PAGE_PAD)
#define RIGHT_PANEL_X  (PAGE_PAD + LEFT_AREA_W + COL_GAP)
#define RIGHT_PANEL_Y  PAGE_PAD
#define RIGHT_PANEL_H  LEFT_AREA_H

/* ─── widgets context ─────────────────────────────────────────────── */

typedef struct {
    /* Tanks */
    lv_obj_t *tank_feed;
    lv_obj_t *tank_inter;
    lv_obj_t *tank_prod;
    /* Pumps */
    lv_obj_t *pump_pre;
    lv_obj_t *pump_st1;
    lv_obj_t *pump_st2;
    /* Sensors */
    lv_obj_t *s_p1, *s_p2, *s_p3, *s_p4;
    lv_obj_t *s_q1, *s_q2, *s_q3, *s_q4;
    lv_obj_t *s_sigma2, *s_sigma3;
    /* KPI cards в правой панели — value labels */
    lv_obj_t *kpi_p3;       /* давление 1-й ст. (bar) */
    lv_obj_t *kpi_p4;       /* давление 2-й ст. (bar) */
    lv_obj_t *kpi_sigma3;   /* проводимость пермеата (μS/cm) */
    lv_obj_t *kpi_q3;       /* расход пермеата (m³/h) */
    lv_obj_t *kpi_temp;     /* температура (°C) */
    lv_obj_t *kpi_sigma1;   /* проводимость питания (μS/cm) */
} mnemonic_widgets_t;

static void widgets_free_cb(lv_event_t *e)
{
    mnemonic_widgets_t *w = lv_event_get_user_data(e);
    if (w) lv_free(w);
}

/* ─── helpers ─────────────────────────────────────────────────────── */

static void fmt_value(char *buf, size_t bufsz, float v, int decimals)
{
    if (isnan(v)) {
        snprintf(buf, bufsz, "—");
    } else {
        snprintf(buf, bufsz, "%.*f", decimals, v);
    }
}

static void update_sensor_value(lv_obj_t *sensor, float v, int decimals)
{
    if (!sensor) return;
    char buf[16];
    fmt_value(buf, sizeof(buf), v, decimals);
    ui_sensor_set_value(sensor, buf);
}

static ui_level_sw_state_t di_to_sw_state(uint8_t di, uint8_t bit_mask)
{
    return (di & bit_mask) ? UI_LEVEL_SW_ACTIVE : UI_LEVEL_SW_DRY;
}

static ui_pump_state_t di_to_pump_state(uint8_t di, uint8_t bit_mask)
{
    return (di & bit_mask) ? UI_PUMP_RUNNING : UI_PUMP_OFF;
}

/* Общий callback: клик на любой sensor circle → открыть детальный модал.
 * Tag и текущее значение приходят из ui_sensor (parsed из value_label). */
static void on_sensor_click(const char *tag, float value)
{
    ui_sensor_modal_show(tag, value);
}

/* Общий handler для клика на оборудование (filter / pump / membrane).
 * user_data — string literal с id оборудования ("filter", "pump-pre", etc.). */
static void on_equipment_click(lv_event_t *e)
{
    const char *id = (const char *)lv_event_get_user_data(e);
    if (id) ui_equipment_modal_show(id);
}

/* Сделать lv_obj кликабельным + повесить on_equipment_click с id. */
static void attach_equipment_click(lv_obj_t *obj, const char *id)
{
    if (!obj) return;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, on_equipment_click, LV_EVENT_CLICKED, (void *)id);
}

/* ─── KPI row helper: "Метка:    Значение [unit]" ─────────────────── */
static lv_obj_t *create_kpi_row(lv_obj_t *parent, const char *label,
                                 const char *unit)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_color(l, ui_token_text_secondary(), 0);
    lv_obj_set_style_text_font(l, UI_FONT_SM, 0);

    /* Значение + unit как inline-сцепка (но реально один label '%v %u' */
    lv_obj_t *v = lv_label_create(row);
    char buf[24];
    snprintf(buf, sizeof(buf), "— %s", unit ? unit : "");
    lv_label_set_text(v, buf);
    lv_obj_set_style_text_color(v, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(v, UI_FONT_MD, 0);
    return v;   /* возвращаем value label для последующих обновлений */
}

/* ─── обновление KPI значения с цветом по состоянию ───────────────── */
static void update_kpi_value(lv_obj_t *val_label, float v, int decimals,
                              const char *unit, lv_color_t color)
{
    if (!val_label) return;
    char buf[24];
    if (isnan(v)) {
        snprintf(buf, sizeof(buf), "— %s", unit ? unit : "");
    } else {
        snprintf(buf, sizeof(buf), "%.*f %s", decimals, v, unit ? unit : "");
    }
    lv_label_set_text(val_label, buf);
    lv_obj_set_style_text_color(val_label, color, 0);
}

/* Классификация значения по двум порогам (warn, alarm) → цвет. */
static lv_color_t classify_color(float v, float warn, float alarm)
{
    if (isnan(v))     return ui_token_text_muted();
    if (v >= alarm)   return ui_token_danger();
    if (v >= warn)    return ui_token_warning();
    return ui_token_accent();
}

/* ─── создание Action button ──────────────────────────────────────── */
static lv_obj_t *create_panel_btn(lv_obj_t *parent, const char *text,
                                   lv_color_t bg_color, lv_color_t text_color,
                                   int height, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, LV_PCT(100), height);
    lv_obj_set_style_bg_color(btn, bg_color, 0);
    lv_obj_set_style_radius(btn, UI_RADIUS_MD, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, text_color, 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_MD, 0);
    lv_obj_center(lbl);
    return btn;
}

/* ─── правая панель: KPI card + Control card ─────────────────────── */
static void create_right_panel(lv_obj_t *parent, mnemonic_widgets_t *w)
{
    /* Контейнер правой панели — flex column с двумя cards. */
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_pos(col, RIGHT_PANEL_X, RIGHT_PANEL_Y);
    lv_obj_set_size(col, RIGHT_PANEL_W, RIGHT_PANEL_H);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, UI_GAP_MD, 0);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    /* ─── KPI card ────────────────────────────────────────────────── */
    lv_obj_t *kpi_card = lv_obj_create(col);
    lv_obj_set_size(kpi_card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(kpi_card, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(kpi_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(kpi_card, ui_token_border(), 0);
    lv_obj_set_style_border_width(kpi_card, 1, 0);
    lv_obj_set_style_radius(kpi_card, UI_RADIUS_LG, 0);
    lv_obj_set_style_pad_all(kpi_card, UI_GAP_MD, 0);
    lv_obj_set_flex_flow(kpi_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(kpi_card, UI_GAP_SM, 0);
    lv_obj_remove_flag(kpi_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *kpi_title = lv_label_create(kpi_card);
    lv_label_set_text(kpi_title, "КЛЮЧЕВЫЕ ПОКАЗАТЕЛИ");
    lv_obj_set_style_text_color(kpi_title, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(kpi_title, UI_FONT_XS, 0);
    lv_obj_set_style_pad_bottom(kpi_title, UI_GAP_XS, 0);

    w->kpi_p3     = create_kpi_row(kpi_card, "P3 (1-я ст.)",      "bar");
    w->kpi_p4     = create_kpi_row(kpi_card, "P4 (2-я ст.)",      "bar");
    w->kpi_sigma3 = create_kpi_row(kpi_card, "\xCF\x83" "3 пермеат", "uS/cm");
    w->kpi_q3     = create_kpi_row(kpi_card, "Q3 пермеат",        "m3/h");
    w->kpi_temp   = create_kpi_row(kpi_card, "Температура",       "°C");
    w->kpi_sigma1 = create_kpi_row(kpi_card, "\xCF\x83" "1 питание",  "uS/cm");

    /* ─── Control card ────────────────────────────────────────────── */
    lv_obj_t *ctrl_card = lv_obj_create(col);
    lv_obj_set_size(ctrl_card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(ctrl_card, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(ctrl_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ctrl_card, ui_token_border(), 0);
    lv_obj_set_style_border_width(ctrl_card, 1, 0);
    lv_obj_set_style_radius(ctrl_card, UI_RADIUS_LG, 0);
    lv_obj_set_style_pad_all(ctrl_card, UI_GAP_MD, 0);
    lv_obj_set_flex_flow(ctrl_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ctrl_card, UI_GAP_SM, 0);
    lv_obj_remove_flag(ctrl_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ctrl_title = lv_label_create(ctrl_card);
    lv_label_set_text(ctrl_title, "УПРАВЛЕНИЕ");
    lv_obj_set_style_text_color(ctrl_title, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(ctrl_title, UI_FONT_XS, 0);
    lv_obj_set_style_pad_bottom(ctrl_title, UI_GAP_XS, 0);

    /* Большая зелёная "Пуск AUTO". */
    create_panel_btn(ctrl_card, "Пуск AUTO",
                     ui_token_accent(), ui_token_text_inverse(),
                     60, ui_evt_start_auto);

    /* "Ручной режим" — обычная высота. */
    create_panel_btn(ctrl_card, "Ручной режим",
                     ui_token_accent(), ui_token_text_inverse(),
                     40, ui_evt_set_manual);

    /* Row: Стоп (danger) + Промывка (secondary). */
    lv_obj_t *row = lv_obj_create(ctrl_card);
    lv_obj_set_size(row, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, UI_GAP_SM, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_stop = lv_button_create(row);
    lv_obj_set_size(btn_stop, LV_PCT(48), 40);
    lv_obj_set_style_bg_color(btn_stop, ui_token_danger(), 0);
    lv_obj_set_style_radius(btn_stop, UI_RADIUS_MD, 0);
    lv_obj_set_style_shadow_width(btn_stop, 0, 0);
    lv_obj_set_style_border_width(btn_stop, 0, 0);
    lv_obj_add_event_cb(btn_stop, ui_evt_stop, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l_stop = lv_label_create(btn_stop);
    lv_label_set_text(l_stop, "Стоп");
    lv_obj_set_style_text_color(l_stop, ui_token_text_inverse(), 0);
    lv_obj_set_style_text_font(l_stop, UI_FONT_MD, 0);
    lv_obj_center(l_stop);

    lv_obj_t *btn_wash = lv_button_create(row);
    lv_obj_set_size(btn_wash, LV_PCT(48), 40);
    lv_obj_set_style_bg_color(btn_wash, ui_token_bg_mute(), 0);
    lv_obj_set_style_radius(btn_wash, UI_RADIUS_MD, 0);
    lv_obj_set_style_shadow_width(btn_wash, 0, 0);
    lv_obj_set_style_border_color(btn_wash, ui_token_border(), 0);
    lv_obj_set_style_border_width(btn_wash, 1, 0);
    lv_obj_add_event_cb(btn_wash, ui_evt_start_washing, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l_wash = lv_label_create(btn_wash);
    lv_label_set_text(l_wash, "Промывка");
    lv_obj_set_style_text_color(l_wash, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(l_wash, UI_FONT_MD, 0);
    lv_obj_center(l_wash);

    /* "Заглушить алярмы". */
    create_panel_btn(ctrl_card, "Заглушить алярмы",
                     ui_token_bg_mute(), ui_token_text_primary(),
                     40, ui_evt_reset_fault);
}

/* ─── собрать мнемосхему по координатам прототипа ─────────────────── */

lv_obj_t *scr_mnemonic_create(lv_obj_t *parent)
{
    /* Корневой контейнер на всю контентную зону. */
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, ui_token_bg_base(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* Контейнер для мнемо — в left area (924 px wide), центрирован.
     * Right panel (KPI + Control) занимает 320 px справа. */
    int canvas_x = PAGE_PAD + (LEFT_AREA_W - MNEMO_PX_W) / 2;
    int canvas_y = PAGE_PAD + (LEFT_AREA_H - MNEMO_PX_H) / 2;

    lv_obj_t *canvas = lv_obj_create(root);
    lv_obj_set_size(canvas, MNEMO_PX_W, MNEMO_PX_H);
    lv_obj_set_pos(canvas, canvas_x, canvas_y);
    lv_obj_set_style_bg_color(canvas, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(canvas, ui_token_border(), 0);
    lv_obj_set_style_border_width(canvas, 1, 0);
    lv_obj_set_style_radius(canvas, UI_RADIUS_LG, 0);
    lv_obj_set_style_pad_all(canvas, 0, 0);
    lv_obj_remove_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

    mnemonic_widgets_t *w = lv_malloc_zeroed(sizeof(*w));
    if (!w) return root;
    lv_obj_set_user_data(root, w);
    lv_obj_add_event_cb(root, widgets_free_cb, LV_EVENT_DELETE, w);

    /* ═══ ТРУБОПРОВОДЫ ═══════════════════════════════════════════════ */

    /* ROW 1 (svg y=210) — поток слева направо, активный */
    ui_pipe_segment(canvas, SX(100), SY(210), SX(152), SY(210), UI_PIPE_ACTIVE);
    ui_pipe_segment(canvas, SX(204), SY(210), SX(310), SY(210), UI_PIPE_ACTIVE);
    ui_pipe_segment(canvas, SX(350), SY(210), SX(452), SY(210), UI_PIPE_ACTIVE);
    ui_pipe_segment(canvas, SX(504), SY(210), SX(590), SY(210), UI_PIPE_ACTIVE);

    /* Дренаж мембраны 1 — серая (вентиль закрыт) */
    ui_pipe_segment(canvas, SX(710), SY(210), SX(790), SY(210), UI_PIPE_INACTIVE);

    /* СВЯЗЬ ROW 1 → ROW 2: пермеат от мембр.1 вниз к промеж. */
    ui_pipe_segment(canvas, SX(650), SY(240), SX(650), SY(320), UI_PIPE_ACTIVE);

    /* ROW 2 (svg y=350) — поток справа налево */
    ui_pipe_segment(canvas, SX(570), SY(350), SX(486), SY(350), UI_PIPE_ACTIVE);
    ui_pipe_segment(canvas, SX(434), SY(350), SX(340), SY(350), UI_PIPE_ACTIVE);
    ui_pipe_segment(canvas, SX(220), SY(350), SX(140), SY(350), UI_PIPE_ACTIVE);

    /* РЕЦИКЛ 1 */
    ui_pipe_segment(canvas, SX(650), SY(180), SX(650), SY(100), UI_PIPE_RECYCLE);
    ui_pipe_segment(canvas, SX(650), SY(100), SX(420), SY(100), UI_PIPE_RECYCLE);
    ui_pipe_segment(canvas, SX(420), SY(100), SX(420), SY(210), UI_PIPE_RECYCLE);

    /* РЕЦИКЛ 2 */
    ui_pipe_segment(canvas, SX(280), SY(380), SX(280), SY(455), UI_PIPE_RECYCLE);
    ui_pipe_segment(canvas, SX(280), SY(455), SX(550), SY(455), UI_PIPE_RECYCLE);
    ui_pipe_segment(canvas, SX(550), SY(455), SX(550), SY(350), UI_PIPE_RECYCLE);

    /* ДОЗАТОР → впрыск ПОСЛЕ фильтра */
    ui_pipe_segment(canvas, SX(216), SY(285), SX(435), SY(285), UI_PIPE_INACTIVE);
    ui_pipe_segment(canvas, SX(435), SY(285), SX(435), SY(210), UI_PIPE_INACTIVE);

    /* Junctions */
    ui_pipe_junction(canvas, SX(420), SY(210));
    ui_pipe_junction(canvas, SX(550), SY(350));
    ui_pipe_junction(canvas, SX(435), SY(210));

    /* ═══ ЁМКОСТИ ═══════════════════════════════════════════════════ */

    ui_tank_config_t feed_cfg = {
        .geom = {
            .x = SX(20), .y = SY(180),
            .w = SX(100) - SX(20), .h = SY(260) - SY(180),
            .fill_pct = 62, .name = "Исходная",
        },
        .switch_count = 1,
        .switches = {
            { .y_offset = SY(245) - SY(180), .tag = "DI1", .state = UI_LEVEL_SW_ACTIVE },
        },
    };
    w->tank_feed = ui_tank_create(canvas, &feed_cfg);

    ui_tank_config_t inter_cfg = {
        .geom = {
            .x = SX(570), .y = SY(320),
            .w = SX(730) - SX(570), .h = SY(400) - SY(320),
            .fill_pct = 69, .name = "Промеж.",
        },
        .switch_count = 2,
        .switches = {
            { .y_offset = SY(332) - SY(320), .tag = "DI3", .state = UI_LEVEL_SW_DRY },
            { .y_offset = SY(388) - SY(320), .tag = "DI2", .state = UI_LEVEL_SW_ACTIVE },
        },
    };
    w->tank_inter = ui_tank_create(canvas, &inter_cfg);

    ui_tank_config_t prod_cfg = {
        .geom = {
            .x = SX(60), .y = SY(320),
            .w = SX(140) - SX(60), .h = SY(400) - SY(320),
            .fill_pct = 62, .name = "Чистая",
        },
        .switch_count = 1,
        .switches = {
            { .y_offset = SY(380) - SY(320), .tag = "DI4", .state = UI_LEVEL_SW_DRY },
        },
    };
    w->tank_prod = ui_tank_create(canvas, &prod_cfg);

    /* ═══ ОБОРУДОВАНИЕ ═══════════════════════════════════════════════ */

    /* Фильтр (кликабельный — открывает модал ΔP + ресурс картриджа) */
    {
        lv_obj_t *filter_obj = ui_filter_create(canvas, SX(330), SY(210), "Фильтр");
        attach_equipment_click(filter_obj, "filter");
    }

    /* Дозатор — простой rect */
    {
        lv_obj_t *doser = lv_obj_create(canvas);
        lv_obj_set_size(doser, SX(216) - SX(140), SY(305) - SY(265));
        lv_obj_set_pos(doser, SX(140), SY(265));
        lv_obj_set_style_radius(doser, UI_RADIUS_MD, 0);
        lv_obj_set_style_bg_color(doser, ui_token_bg_mute(), 0);
        lv_obj_set_style_bg_opa(doser, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(doser, ui_token_border_strong(), 0);
        lv_obj_set_style_border_width(doser, 2, 0);
        lv_obj_set_style_pad_all(doser, 0, 0);
        lv_obj_remove_flag(doser, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *lbl = lv_label_create(doser);
        lv_label_set_text(lbl, "Дозатор");
        lv_obj_set_style_text_color(lbl, ui_token_text_primary(), 0);
        lv_obj_set_style_text_font(lbl, UI_FONT_XS, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -6);
        lv_obj_t *lbl2 = lv_label_create(doser);
        lv_label_set_text(lbl2, "RO5 OFF");   /* '·' middle-dot отсутствует в Montserrat → используем пробел */
        lv_obj_set_style_text_color(lbl2, ui_token_text_secondary(), 0);
        lv_obj_set_style_text_font(lbl2, UI_FONT_XS, 0);
        lv_obj_align(lbl2, LV_ALIGN_CENTER, 0, 6);
    }

    /* Насосы (кликабельные — открывают модал состояния и моточасов) */
    ui_pump_config_t pump_cfg;

    pump_cfg = (ui_pump_config_t){
        .cx = SX(178), .cy = SY(210),
        .label = "Преднагн.", .state = UI_PUMP_RUNNING,
    };
    w->pump_pre = ui_pump_create(canvas, &pump_cfg);
    attach_equipment_click(w->pump_pre, "pump-pre");

    pump_cfg = (ui_pump_config_t){
        .cx = SX(478), .cy = SY(210),
        .label = "1-я ст.", .state = UI_PUMP_RUNNING,
    };
    w->pump_st1 = ui_pump_create(canvas, &pump_cfg);
    attach_equipment_click(w->pump_st1, "pump-st1");

    pump_cfg = (ui_pump_config_t){
        .cx = SX(460), .cy = SY(350),
        .label = "2-я ст.", .state = UI_PUMP_RUNNING,
    };
    w->pump_st2 = ui_pump_create(canvas, &pump_cfg);
    attach_equipment_click(w->pump_st2, "pump-st2");

    /* Мембраны (кликабельные — модал rejection + наработка от промывки) */
    ui_membrane_config_t m1 = {
        .x = SX(590), .y = SY(180),
        .w = SX(710) - SX(590), .h = SY(240) - SY(180),
        .label = "Мембрана 1ст.",
    };
    attach_equipment_click(ui_membrane_create(canvas, &m1), "membrane-1");

    ui_membrane_config_t m2 = {
        .x = SX(220), .y = SY(320),
        .w = SX(340) - SX(220), .h = SY(380) - SY(320),
        .label = "Мембрана 2ст.",
    };
    attach_equipment_click(ui_membrane_create(canvas, &m2), "membrane-2");

    /* ═══ ДАТЧИКИ ═══════════════════════════════════════════════════ */

    ui_sensor_config_t sc;

    /* P1 (235, 135) tap (235, 210) */
    sc = (ui_sensor_config_t){
        .tap_x = SX(235), .tap_y = SY(210),
        .circle_x = SX(235), .circle_y = SY(135),
        .tag = "P1", .value = "3.2", .state = UI_SENSOR_OK,
    };
    w->s_p1 = ui_sensor_create(canvas, &sc);

    /* Q1 (290, 135) */
    sc = (ui_sensor_config_t){
        .tap_x = SX(290), .tap_y = SY(210),
        .circle_x = SX(290), .circle_y = SY(135),
        .tag = "Q1", .value = "2.5", .state = UI_SENSOR_OK,
    };
    w->s_q1 = ui_sensor_create(canvas, &sc);

    /* P2 (385, 135) */
    sc = (ui_sensor_config_t){
        .tap_x = SX(385), .tap_y = SY(210),
        .circle_x = SX(385), .circle_y = SY(135),
        .tag = "P2", .value = "2.9", .state = UI_SENSOR_OK,
    };
    w->s_p2 = ui_sensor_create(canvas, &sc);

    /* P3 (553, 135) */
    sc = (ui_sensor_config_t){
        .tap_x = SX(553), .tap_y = SY(210),
        .circle_x = SX(553), .circle_y = SY(135),
        .tag = "P3", .value = "26.8", .state = UI_SENSOR_OK,
    };
    w->s_p3 = ui_sensor_create(canvas, &sc);

    /* Q2 (535, 55) — на recycle-1 */
    sc = (ui_sensor_config_t){
        .tap_x = SX(535), .tap_y = SY(100),
        .circle_x = SX(535), .circle_y = SY(55),
        .tag = "Q2", .value = "0.8", .state = UI_SENSOR_OK,
    };
    w->s_q2 = ui_sensor_create(canvas, &sc);

    /* σ2 (735, 280) — пермеат мембр.1 */
    sc = (ui_sensor_config_t){
        .tap_x = SX(650), .tap_y = SY(280),
        .circle_x = SX(735), .circle_y = SY(280),
        .tag = "\xCF\x83" "2",   /* "σ2" */
        .value = "18.4", .state = UI_SENSOR_OK,
    };
    w->s_sigma2 = ui_sensor_create(canvas, &sc);

    /* P4 (380, 425) */
    sc = (ui_sensor_config_t){
        .tap_x = SX(380), .tap_y = SY(350),
        .circle_x = SX(380), .circle_y = SY(425),
        .tag = "P4", .value = "6.5", .state = UI_SENSOR_OK,
    };
    w->s_p4 = ui_sensor_create(canvas, &sc);

    /* σ3 (155, 425) — товарный пермеат */
    sc = (ui_sensor_config_t){
        .tap_x = SX(155), .tap_y = SY(350),
        .circle_x = SX(155), .circle_y = SY(425),
        .tag = "\xCF\x83" "3",   /* "σ3" */
        .value = "1.2", .state = UI_SENSOR_OK,
    };
    w->s_sigma3 = ui_sensor_create(canvas, &sc);

    /* Q3 (210, 425) */
    sc = (ui_sensor_config_t){
        .tap_x = SX(210), .tap_y = SY(350),
        .circle_x = SX(210), .circle_y = SY(425),
        .tag = "Q3", .value = "1.4", .state = UI_SENSOR_OK,
    };
    w->s_q3 = ui_sensor_create(canvas, &sc);

    /* Q4 (415, 505) — рецикл 2 */
    sc = (ui_sensor_config_t){
        .tap_x = SX(415), .tap_y = SY(455),
        .circle_x = SX(415), .circle_y = SY(505),
        .tag = "Q4", .value = "0.5", .state = UI_SENSOR_OK,
    };
    w->s_q4 = ui_sensor_create(canvas, &sc);

    /* Регистрация click-handler на каждый sensor circle — открывает
     * подробный модал по клику. Tag и значение читаются из ui_sensor. */
    ui_sensor_set_click_cb(w->s_p1, on_sensor_click);
    ui_sensor_set_click_cb(w->s_p2, on_sensor_click);
    ui_sensor_set_click_cb(w->s_p3, on_sensor_click);
    ui_sensor_set_click_cb(w->s_p4, on_sensor_click);
    ui_sensor_set_click_cb(w->s_q1, on_sensor_click);
    ui_sensor_set_click_cb(w->s_q2, on_sensor_click);
    ui_sensor_set_click_cb(w->s_q3, on_sensor_click);
    ui_sensor_set_click_cb(w->s_q4, on_sensor_click);
    ui_sensor_set_click_cb(w->s_sigma2, on_sensor_click);
    ui_sensor_set_click_cb(w->s_sigma3, on_sensor_click);

    /* ═══ ПРАВАЯ ПАНЕЛЬ: KPI + Controls ════════════════════════════ */
    create_right_panel(root, w);

    return root;
}

/* ─── обновление по plant_data_t ─────────────────────────────────── */

void scr_mnemonic_update(lv_obj_t *container, const plant_data_t *data, uint32_t dirty)
{
    if (!container || !data) return;
    mnemonic_widgets_t *w = lv_obj_get_user_data(container);
    if (!w) return;

    /* Аналоговые датчики (давление). */
    if (dirty & DIRTY_ANALOG) {
        update_sensor_value(w->s_p1, data->pressure[0].value, 1);
        update_sensor_value(w->s_p2, data->pressure[1].value, 1);
        update_sensor_value(w->s_p3, data->pressure[2].value, 1);
        update_sensor_value(w->s_p4, data->pressure[3].value, 1);

        /* KPI: P3 и P4 — те же значения с состоянием по уставкам. */
        update_kpi_value(w->kpi_p3, data->pressure[2].value, 1, "bar",
            classify_color(data->pressure[2].value, 32, 35));
        update_kpi_value(w->kpi_p4, data->pressure[3].value, 1, "bar",
            classify_color(data->pressure[3].value, 7.5f, 8.0f));

        /* Температура (data->temperature.value). */
        update_kpi_value(w->kpi_temp, data->temperature.value, 1, "°C",
            classify_color(data->temperature.value, 30, 35));
    }

    /* Расходомеры. */
    if (dirty & DIRTY_FLOW) {
        update_sensor_value(w->s_q1, data->flow[0].flow, 2);
        update_sensor_value(w->s_q2, data->flow[1].flow, 2);
        update_sensor_value(w->s_q3, data->flow[2].flow, 2);
        update_sensor_value(w->s_q4, data->flow[3].flow, 2);

        /* KPI: Q3 — расход товарного пермеата. */
        update_kpi_value(w->kpi_q3, data->flow[2].flow, 2, "m3/h",
            ui_token_accent());  /* без warn/alarm — informational */
    }

    /* Кондуктометры (s2 = PERM1 = σ2, s3 = PERM2 = σ3). */
    if (dirty & DIRTY_CONDUCTIVITY) {
        update_sensor_value(w->s_sigma2, data->conductivity[1].conductivity, 1);
        update_sensor_value(w->s_sigma3, data->conductivity[2].conductivity, 1);

        /* KPI: σ3 — проводимость товарного пермеата (важный показатель),
         * σ1 — проводимость питательной воды (контекст). */
        update_kpi_value(w->kpi_sigma3, data->conductivity[2].conductivity, 1, "uS/cm",
            classify_color(data->conductivity[2].conductivity, 5, 10));
        update_kpi_value(w->kpi_sigma1, data->conductivity[0].conductivity, 0, "uS/cm",
            ui_token_text_secondary());  /* informational */
    }

    /* IO: поплавки уровня + статус насосов + уровень воды в ёмкостях.
     * Маппинг proto-меток на hardware DI-биты:
     *   "DI1" (Исходная low)  ↔ DI_SOURCE_LOW
     *   "DI3" (Промеж high)   ↔ DI_INTERM_HIGH
     *   "DI2" (Промеж low)    ↔ DI_INTERM_LOW
     *   "DI4" (Чистая high)   ↔ DI_PERMEATE_HIGH
     *
     * Высота столба воды визуализирует state поплавков (на железе
     * нет непрерывного level sensor — только 1-2 дискретных уровня).
     * Mapping:
     *   ИСХОДНАЯ:    low dry → 10%,  low wet → 60%
     *   ПРОМЕЖ:      low dry → 10%,  low wet+high dry → 55%,  high wet → 90%
     *   ЧИСТАЯ:      high dry → 50%, high wet → 90%
     */
    if (dirty & DIRTY_IO) {
        bool src_low    = (data->di & DI_SOURCE_LOW) != 0;
        bool int_low    = (data->di & DI_INTERM_LOW) != 0;
        bool int_high   = (data->di & DI_INTERM_HIGH) != 0;
        bool prod_high  = (data->di & DI_PERMEATE_HIGH) != 0;

        if (w->tank_feed) {
            ui_tank_set_switch_state(w->tank_feed, 0,
                src_low ? UI_LEVEL_SW_ACTIVE : UI_LEVEL_SW_DRY);
            ui_tank_set_fill(w->tank_feed, src_low ? 60 : 10);
        }
        if (w->tank_inter) {
            ui_tank_set_switch_state(w->tank_inter, 0,
                int_high ? UI_LEVEL_SW_ACTIVE : UI_LEVEL_SW_DRY);
            ui_tank_set_switch_state(w->tank_inter, 1,
                int_low ? UI_LEVEL_SW_ACTIVE : UI_LEVEL_SW_DRY);
            int inter_fill = int_high ? 90 : (int_low ? 55 : 10);
            ui_tank_set_fill(w->tank_inter, inter_fill);
        }
        if (w->tank_prod) {
            ui_tank_set_switch_state(w->tank_prod, 0,
                prod_high ? UI_LEVEL_SW_ACTIVE : UI_LEVEL_SW_DRY);
            ui_tank_set_fill(w->tank_prod, prod_high ? 90 : 50);
        }

        if (w->pump_pre) {
            ui_pump_set_state(w->pump_pre, di_to_pump_state(data->di, DI_PUMP1_RUN));
        }
        if (w->pump_st1) {
            ui_pump_set_state(w->pump_st1, di_to_pump_state(data->di, DI_PUMP2_RUN));
        }
        if (w->pump_st2) {
            ui_pump_set_state(w->pump_st2, di_to_pump_state(data->di, DI_PUMP3_RUN));
        }
    }
}
