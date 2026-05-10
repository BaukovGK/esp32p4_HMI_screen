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
#include "lang.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ─── coord helpers ─────────────────────────────────────────────────
 * SX / SY — конвертеры SVG-координат прототипа в физические пиксели.
 * Прототип использует viewBox "0 30 900 530" — вычитаем 30 из y. */
#define SX(svg_x) MNEMO_PX(svg_x)
#define SY(svg_y) MNEMO_PX((svg_y) - 30)

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

    /* Контейнер для мнемо — центрируется горизонтально (letterbox 52px по бокам). */
    lv_obj_t *canvas = lv_obj_create(root);
    lv_obj_set_size(canvas, MNEMO_PX_W, MNEMO_PX_H);
    lv_obj_set_pos(canvas, (UI_SCREEN_W - MNEMO_PX_W) / 2, 0);
    lv_obj_set_style_bg_color(canvas, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(canvas, 0, 0);
    lv_obj_set_style_radius(canvas, 0, 0);
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
    }

    /* Расходомеры. */
    if (dirty & DIRTY_FLOW) {
        update_sensor_value(w->s_q1, data->flow[0].flow, 2);
        update_sensor_value(w->s_q2, data->flow[1].flow, 2);
        update_sensor_value(w->s_q3, data->flow[2].flow, 2);
        update_sensor_value(w->s_q4, data->flow[3].flow, 2);
    }

    /* Кондуктометры (s2 = PERM1 = σ2, s3 = PERM2 = σ3). */
    if (dirty & DIRTY_CONDUCTIVITY) {
        update_sensor_value(w->s_sigma2, data->conductivity[1].conductivity, 1);
        update_sensor_value(w->s_sigma3, data->conductivity[2].conductivity, 1);
    }

    /* IO: поплавки уровня + статус насосов.
     * Маппинг proto-меток на hardware DI-биты:
     *   "DI1" (Исходная low)  ↔ DI_SOURCE_LOW
     *   "DI3" (Промеж high)   ↔ DI_INTERM_HIGH
     *   "DI2" (Промеж low)    ↔ DI_INTERM_LOW
     *   "DI4" (Чистая high)   ↔ DI_PERMEATE_HIGH */
    if (dirty & DIRTY_IO) {
        if (w->tank_feed) {
            ui_tank_set_switch_state(w->tank_feed, 0,
                di_to_sw_state(data->di, DI_SOURCE_LOW));
        }
        if (w->tank_inter) {
            ui_tank_set_switch_state(w->tank_inter, 0,
                di_to_sw_state(data->di, DI_INTERM_HIGH));
            ui_tank_set_switch_state(w->tank_inter, 1,
                di_to_sw_state(data->di, DI_INTERM_LOW));
        }
        if (w->tank_prod) {
            ui_tank_set_switch_state(w->tank_prod, 0,
                di_to_sw_state(data->di, DI_PERMEATE_HIGH));
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
