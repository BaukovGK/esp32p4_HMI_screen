/**
 * @file scr_diagnostics.c
 * @brief Экран "Отладка" — переработан под proto/debug.html.
 *
 * Layout (1280×692):
 *   Left log panel (~924 px):
 *     - Header: "Журнал событий"
 *     - Scrollable list of alarm rows (time, msg, badge)
 *   Right column (320 px):
 *     - Card: Диагностика (uptime, heap, mqtt status)
 *     - Card: Modbus устройства (table with status badges)
 */

#include "scr_diagnostics.h"
#include "ui_tokens.h"
#include "ui_fonts.h"
#include "ui_common.h"   /* ui_fmt_uptime */
#include "lang.h"
#include "alarm_ring.h"
#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#define PAGE_PAD       12
#define COL_GAP        12
#define RIGHT_W        320
#define LEFT_W         (UI_SCREEN_W - 2 * PAGE_PAD - COL_GAP - RIGHT_W)
#define INNER_H        (UI_CONTENT_H - 2 * PAGE_PAD)

#define MB_DEVICE_COUNT 6

typedef struct {
    lv_obj_t *lbl_uptime;
    lv_obj_t *lbl_heap_free;
    lv_obj_t *lbl_heap_min;
    lv_obj_t *lbl_mqtt_status;
    lv_obj_t *mb_badge[MB_DEVICE_COUNT];
    lv_obj_t *mb_err[MB_DEVICE_COUNT];
} debug_widgets_t;

static void widgets_free_cb(lv_event_t *e)
{
    void *p = lv_obj_get_user_data(lv_event_get_target(e));
    if (p) lv_free(p);
}

/* ─── helpers ─────────────────────────────────────────────────────── */

static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h,
                            const char *title)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, ui_token_border(), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, UI_RADIUS_LG, 0);
    lv_obj_set_style_pad_all(card, UI_GAP_MD, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, UI_GAP_XS, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (title) {
        lv_obj_t *t = lv_label_create(card);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_color(t, ui_token_text_muted(), 0);
        lv_obj_set_style_text_font(t, UI_FONT_XS, 0);
        lv_obj_set_style_pad_bottom(t, UI_GAP_XS, 0);
    }
    return card;
}

static lv_obj_t *make_kv_row(lv_obj_t *parent, const char *key,
                              const char *initial)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *k = lv_label_create(row);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_color(k, ui_token_text_secondary(), 0);
    lv_obj_set_style_text_font(k, UI_FONT_SM, 0);

    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, initial ? initial : "—");
    lv_obj_set_style_text_color(v, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(v, UI_FONT_SM, 0);
    return v;
}

/* ─── alarm row in log panel ──────────────────────────────────────── */

static const char *cat_badge_text(alarm_category_t c)
{
    switch (c) {
    case ALARM_CAT_CRITICAL: return "CRIT";
    case ALARM_CAT_ALARM:    return "ALARM";
    case ALARM_CAT_WARNING:  return "WARN";
    default:                 return "INFO";
    }
}

static lv_color_t cat_color(alarm_category_t c)
{
    switch (c) {
    case ALARM_CAT_CRITICAL:
    case ALARM_CAT_ALARM:   return ui_token_danger();
    case ALARM_CAT_WARNING: return ui_token_warning();
    default:                return ui_token_text_muted();
    }
}

static void add_alarm_row(lv_obj_t *list, const alarm_entry_t *a)
{
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, a->active ? ui_token_bg_mute() : ui_token_bg_card(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(row, cat_color(a->cat), 0);
    lv_obj_set_style_border_width(row, 3, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_radius(row, UI_RADIUS_SM, 0);
    lv_obj_set_style_pad_all(row, UI_GAP_SM, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, UI_GAP_MD, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Time (uptime → HH:MM:SS). */
    char tbuf[12];
    int total_s = (int)(a->ts / 1000000);
    snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d",
             (total_s / 3600) % 24, (total_s / 60) % 60, total_s % 60);
    lv_obj_t *time_lbl = lv_label_create(row);
    lv_label_set_text(time_lbl, tbuf);
    lv_obj_set_style_text_color(time_lbl, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(time_lbl, UI_FONT_XS, 0);
    lv_obj_set_width(time_lbl, 70);

    /* Category + code. */
    char mbuf[80];
    snprintf(mbuf, sizeof(mbuf), "%s — %s", cat_badge_text(a->cat), a->code);
    lv_obj_t *msg = lv_label_create(row);
    lv_label_set_text(msg, mbuf);
    lv_obj_set_style_text_color(msg, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(msg, UI_FONT_SM, 0);
    lv_obj_set_flex_grow(msg, 1);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_DOT);

    /* Badge. */
    lv_obj_t *badge = lv_obj_create(row);
    lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(badge, cat_color(a->cat), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_hor(badge, UI_GAP_SM, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *bl = lv_label_create(badge);
    lv_label_set_text(bl, cat_badge_text(a->cat));
    lv_obj_set_style_text_color(bl, ui_token_text_inverse(), 0);
    lv_obj_set_style_text_font(bl, UI_FONT_XS, 0);
}

/* ─── modbus row ─────────────────────────────────────────────────── */

static const struct {
    int         addr;
    const char *name;
} MB_DEVICES[MB_DEVICE_COUNT] = {
    {  1, "Waveshare AI" },
    {  2, "URZh2KM"      },
    { 10, "SL21 #1"      },
    { 11, "SL21 #2"      },
    { 20, "KWS LP"       },
    { 21, "KWS HP"       },
};

static void create_mb_row(lv_obj_t *parent, int idx, debug_widgets_t *w)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, UI_GAP_SM, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    char abuf[8];
    snprintf(abuf, sizeof(abuf), "%d", MB_DEVICES[idx].addr);
    lv_obj_t *addr = lv_label_create(row);
    lv_label_set_text(addr, abuf);
    lv_obj_set_style_text_color(addr, ui_token_text_muted(), 0);
    lv_obj_set_style_text_font(addr, UI_FONT_SM, 0);
    lv_obj_set_width(addr, 28);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, MB_DEVICES[idx].name);
    lv_obj_set_style_text_color(name, ui_token_text_primary(), 0);
    lv_obj_set_style_text_font(name, UI_FONT_SM, 0);
    lv_obj_set_flex_grow(name, 1);

    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, ui_token_text_muted(), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    w->mb_badge[idx] = dot;

    lv_obj_t *err = lv_label_create(row);
    lv_label_set_text(err, "0");
    lv_obj_set_style_text_color(err, ui_token_text_secondary(), 0);
    lv_obj_set_style_text_font(err, UI_FONT_SM, 0);
    lv_obj_set_width(err, 28);
    lv_obj_set_style_text_align(err, LV_TEXT_ALIGN_RIGHT, 0);
    w->mb_err[idx] = err;
}

/* ─── create screen ──────────────────────────────────────────────── */

lv_obj_t *scr_diagnostics_create(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_set_size(root, UI_SCREEN_W, UI_CONTENT_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, ui_token_bg_base(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    debug_widgets_t *w = lv_malloc_zeroed(sizeof(*w));
    if (!w) return root;
    lv_obj_set_user_data(root, w);
    lv_obj_add_event_cb(root, widgets_free_cb, LV_EVENT_DELETE, NULL);

    /* Left: log panel. */
    lv_obj_t *log_card = make_card(root, PAGE_PAD, PAGE_PAD, LEFT_W, INNER_H,
                                    "ЖУРНАЛ СОБЫТИЙ");

    /* Log list — scrollable. */
    lv_obj_t *list = lv_obj_create(log_card);
    lv_obj_set_size(list, LV_PCT(100), INNER_H - 64);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, UI_GAP_XS, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    /* Заполнить активными авариями. */
    alarm_entry_t entries[16];
    int n = alarm_ring_get_active(entries, 16);
    for (int i = 0; i < n; i++) {
        add_alarm_row(list, &entries[i]);
    }
    if (n == 0) {
        lv_obj_t *empty = lv_label_create(list);
        lv_label_set_text(empty, "Нет активных событий");
        lv_obj_set_style_text_color(empty, ui_token_text_muted(), 0);
        lv_obj_set_style_text_font(empty, UI_FONT_MD, 0);
        lv_obj_set_style_pad_all(empty, UI_GAP_LG, 0);
    }

    /* Right column. */
    int right_x = PAGE_PAD + LEFT_W + COL_GAP;
    int right_y = PAGE_PAD;

    int diag_h = 260;
    lv_obj_t *diag_card = make_card(root, right_x, right_y, RIGHT_W, diag_h,
                                     "ДИАГНОСТИКА");
    make_kv_row(diag_card, "Прошивка", "v1.0.0-dev");
    make_kv_row(diag_card, "Контроллер", "—");
    w->lbl_uptime    = make_kv_row(diag_card, "Uptime",       "00:00:00");
    w->lbl_heap_free = make_kv_row(diag_card, "Heap свободно", "0 B");
    w->lbl_heap_min  = make_kv_row(diag_card, "Heap мин.",     "0 B");
    w->lbl_mqtt_status = make_kv_row(diag_card, "MQTT", "—");

    right_y += diag_h + UI_GAP_MD;
    int mb_h = INNER_H - diag_h - UI_GAP_MD;
    lv_obj_t *mb_card = make_card(root, right_x, right_y, RIGHT_W, mb_h,
                                   "MODBUS УСТРОЙСТВА");
    for (int i = 0; i < MB_DEVICE_COUNT; i++) {
        create_mb_row(mb_card, i, w);
    }

    return root;
}

/* ─── update ─────────────────────────────────────────────────────── */

void scr_diagnostics_update(lv_obj_t *container, const plant_data_t *data, uint32_t dirty)
{
    (void)dirty;
    if (!container || !data) return;
    debug_widgets_t *w = lv_obj_get_user_data(container);
    if (!w) return;

    char buf[32];

    if (w->lbl_uptime) {
        ui_fmt_uptime(buf, sizeof(buf), data->diagnostics.uptime_s);
        lv_label_set_text(w->lbl_uptime, buf);
    }
    if (w->lbl_heap_free) {
        snprintf(buf, sizeof(buf), "%" PRIu32 " B", data->diagnostics.heap_free);
        lv_label_set_text(w->lbl_heap_free, buf);
        lv_obj_set_style_text_color(w->lbl_heap_free,
            data->diagnostics.heap_free < 32 * 1024 ? ui_token_danger()
                                                    : ui_token_text_primary(), 0);
    }
    if (w->lbl_heap_min) {
        snprintf(buf, sizeof(buf), "%" PRIu32 " B", data->diagnostics.heap_min);
        lv_label_set_text(w->lbl_heap_min, buf);
    }
    if (w->lbl_mqtt_status) {
        lv_label_set_text(w->lbl_mqtt_status,
            data->mqtt_connected ? "Подключено" : "Нет связи");
        lv_obj_set_style_text_color(w->lbl_mqtt_status,
            data->mqtt_connected ? ui_token_success() : ui_token_danger(), 0);
    }

    /* Modbus device status. */
    for (int i = 0; i < MB_DEVICE_COUNT && i < DIAG_MAX_MB_DEVICES; i++) {
        bool online = data->diagnostics.modbus_online[i];
        uint32_t errs = data->diagnostics.modbus_errors[i];
        if (w->mb_badge[i]) {
            lv_obj_set_style_bg_color(w->mb_badge[i],
                online ? (errs > 0 ? ui_token_warning() : ui_token_success())
                       : ui_token_danger(), 0);
        }
        if (w->mb_err[i]) {
            snprintf(buf, sizeof(buf), "%" PRIu32, errs);
            lv_label_set_text(w->mb_err[i], buf);
            lv_obj_set_style_text_color(w->mb_err[i],
                errs > 0 ? ui_token_warning() : ui_token_text_secondary(), 0);
        }
    }
}
