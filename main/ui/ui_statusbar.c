/**
 * @file ui_statusbar.c
 * @brief Реализация верхней статус-полосы (44px) — порт .statusbar из прототипа.
 */
#include "ui_statusbar.h"
#include "ui_tokens.h"
#include "ui_fonts.h"
#include "lang.h"
#include "alarm_ring.h"
#include "board.h"
#include <stdio.h>
#include <time.h>

/** Контекст полосы — все дочерние виджеты для последующего обновления. */
typedef struct {
    lv_obj_t *time_label;       /* HH:MM:SS, слева */
    lv_obj_t *status_label;     /* текст состояния/аварии, flex-grow */
    lv_obj_t *conn_dot;         /* кружок индикатора связи */
    lv_obj_t *conn_label;       /* "Связь с хабом" / "Нет связи" */
    lv_obj_t *theme_btn;        /* ☾ / ☀ */
    lv_obj_t *theme_btn_label;  /* для смены глифа при переключении */
    lv_obj_t *lang_btn;         /* RU / EN */
    lv_obj_t *lang_btn_label;
    bool      stale;            /* флаг «нет данных» */
    ui_statusbar_theme_cb_t on_theme_change;
} statusbar_ctx_t;

static statusbar_ctx_t s_sb = {0};

/* ─── обработчики кнопок ──────────────────────────────────────────── */

static void theme_btn_cb(lv_event_t *e)
{
    (void)e;
    ui_theme_id_t cur = ui_tokens_get_theme();
    ui_theme_id_t next = (cur == UI_THEME_DARK) ? UI_THEME_LIGHT : UI_THEME_DARK;
    ui_tokens_set_theme(next);

    /* Сменить глиф ☾↔☀ */
    if (s_sb.theme_btn_label) {
        lv_label_set_text(s_sb.theme_btn_label, next == UI_THEME_DARK ? "\xE2\x98\x80" : "\xE2\x98\xBE");
        /* ☀ = U+2600 (e2 98 80), ☾ = U+263E (e2 98 be) */
    }

    /* Поднять обратно — пусть main пересоздаст экраны со свежими цветами. */
    if (s_sb.on_theme_change) s_sb.on_theme_change();
}

static void lang_btn_cb(lv_event_t *e)
{
    (void)e;
    lang_set(lang_get() == LANG_RU ? LANG_EN : LANG_RU);
    if (s_sb.lang_btn_label) {
        lv_label_set_text(s_sb.lang_btn_label, lang_get() == LANG_RU ? "RU" : "EN");
    }
    /* Текущие тексты на других виджетах обновятся при следующем refresh-tick'е
     * (или после ui_*_set_text-апдейта в screens). */
}

/* ─── вспомогательные функции для оформления элементов ────────────── */

/** Унифицированная тач-кнопка sb-btn (как в прототипе). */
static lv_obj_t *make_sb_btn(lv_obj_t *parent, const char *glyph,
                             lv_event_cb_t cb, lv_obj_t **out_label)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 48, 32);
    lv_obj_set_style_radius(btn, UI_RADIUS_SM, 0);
    lv_obj_set_style_bg_color(btn, ui_token_bg_mute(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, ui_token_border(), 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, glyph);
    lv_obj_set_style_text_color(lbl, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(lbl, UI_FONT_SM, 0);
    lv_obj_center(lbl);
    if (out_label) *out_label = lbl;

    return btn;
}

/* ─── публичный API ───────────────────────────────────────────────── */

lv_obj_t *ui_statusbar_create(lv_obj_t *parent, ui_statusbar_theme_cb_t on_theme)
{
    s_sb.on_theme_change = on_theme;

    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_STATUSBAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, ui_token_border(), 0);
    lv_obj_set_style_pad_hor(bar, UI_GAP_LG, 0);
    lv_obj_set_style_pad_ver(bar, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, UI_GAP_LG, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* 1. Время HH:MM:SS — слева, фиксированная ширина */
    s_sb.time_label = lv_label_create(bar);
    lv_label_set_text(s_sb.time_label, "--:--:--");
    lv_obj_set_style_text_color(s_sb.time_label, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(s_sb.time_label, UI_FONT_MD, 0);
    lv_obj_set_width(s_sb.time_label, 80);

    /* 2. Статус-сообщение — flex-grow */
    s_sb.status_label = lv_label_create(bar);
    lv_label_set_text(s_sb.status_label, "");
    lv_obj_set_style_text_color(s_sb.status_label, ui_token_text_secondary(), 0);
    lv_obj_set_style_text_font(s_sb.status_label, UI_FONT_SM, 0);
    lv_obj_set_flex_grow(s_sb.status_label, 1);
    lv_label_set_long_mode(s_sb.status_label, LV_LABEL_LONG_DOT);

    /* 3. Индикатор связи: контейнер с dot + label */
    lv_obj_t *conn_box = lv_obj_create(bar);
    lv_obj_set_size(conn_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(conn_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(conn_box, 0, 0);
    lv_obj_set_style_pad_all(conn_box, 0, 0);
    lv_obj_set_style_pad_column(conn_box, UI_GAP_XS + 2, 0);
    lv_obj_set_flex_flow(conn_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(conn_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(conn_box, LV_OBJ_FLAG_SCROLLABLE);

    s_sb.conn_dot = lv_obj_create(conn_box);
    lv_obj_set_size(s_sb.conn_dot, 8, 8);
    lv_obj_set_style_radius(s_sb.conn_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_sb.conn_dot, ui_token_success(), 0);
    lv_obj_set_style_bg_opa(s_sb.conn_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_sb.conn_dot, 0, 0);
    lv_obj_set_style_pad_all(s_sb.conn_dot, 0, 0);

    s_sb.conn_label = lv_label_create(conn_box);
    lv_label_set_text(s_sb.conn_label, "Связь с хабом");
    lv_obj_set_style_text_color(s_sb.conn_label, ui_token_text_secondary(), 0);
    lv_obj_set_style_text_font(s_sb.conn_label, UI_FONT_SM, 0);

    /* 4. Кнопка темы (☾/☀) */
    s_sb.theme_btn = make_sb_btn(bar,
                                 ui_tokens_get_theme() == UI_THEME_DARK ? "\xE2\x98\x80" : "\xE2\x98\xBE",
                                 theme_btn_cb,
                                 &s_sb.theme_btn_label);

    /* 5. Кнопка языка (RU/EN) */
    s_sb.lang_btn = make_sb_btn(bar,
                                lang_get() == LANG_RU ? "RU" : "EN",
                                lang_btn_cb,
                                &s_sb.lang_btn_label);

    return bar;
}

void ui_statusbar_set_stale(lv_obj_t *bar, bool stale)
{
    (void)bar;
    s_sb.stale = stale;
}

void ui_statusbar_update(lv_obj_t *bar, const plant_data_t *data)
{
    (void)bar;
    if (!data) return;

    /* Время — берём из системных часов (RTC синхронизирован через NTP/MQTT). */
    if (s_sb.time_label) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        lv_label_set_text(s_sb.time_label, buf);
    }

    /* Индикатор соединения (MQTT). */
    if (s_sb.conn_dot && s_sb.conn_label) {
        bool online = data->mqtt_connected && !s_sb.stale;
        lv_obj_set_style_bg_color(s_sb.conn_dot,
            online ? ui_token_success() : ui_token_danger(), 0);
        lv_label_set_text(s_sb.conn_label,
            online ? lang_str(STR_STATUS_ONLINE) : lang_str(STR_STATUS_OFFLINE));
    }

    /* Статус-сообщение: при stale → ошибка, иначе — режим установки или
     * код наиболее критичной активной аварии. */
    if (s_sb.status_label) {
        if (s_sb.stale) {
            lv_label_set_text(s_sb.status_label, lang_str(STR_STATUS_NO_DATA));
            lv_obj_set_style_text_color(s_sb.status_label, ui_token_danger(), 0);
        } else {
            alarm_record_t worst;
            if (alarm_ring_get_worst(&worst) && worst.active) {
                lv_label_set_text(s_sb.status_label, worst.code);
                lv_color_t color;
                switch (worst.cat) {
                case ALARM_CAT_CRITICAL:
                case ALARM_CAT_ALARM:    color = ui_token_danger();  break;
                case ALARM_CAT_WARNING:  color = ui_token_warning(); break;
                default:                 color = ui_token_text_secondary(); break;
                }
                lv_obj_set_style_text_color(s_sb.status_label, color, 0);
            } else {
                /* Нормальное состояние — показываем режим. */
                const char *mode_str = lang_str(STR_MODE_UNKNOWN);
                switch (data->state) {
                case PLANT_STATE_IDLE:    mode_str = lang_str(STR_MODE_IDLE);    break;
                case PLANT_STATE_AUTO:    mode_str = lang_str(STR_MODE_AUTO);    break;
                case PLANT_STATE_WASHING: mode_str = lang_str(STR_MODE_WASHING); break;
                case PLANT_STATE_MANUAL:  mode_str = lang_str(STR_MODE_MANUAL);  break;
                case PLANT_STATE_FAULT:   mode_str = lang_str(STR_MODE_FAULT);   break;
                default: break;
                }
                lv_label_set_text(s_sb.status_label, mode_str);
                lv_color_t color = (data->state == PLANT_STATE_FAULT)
                    ? ui_token_danger()
                    : (data->state == PLANT_STATE_WASHING) ? ui_token_warning()
                                                            : ui_token_text_secondary();
                lv_obj_set_style_text_color(s_sb.status_label, color, 0);
            }
        }
    }
}
