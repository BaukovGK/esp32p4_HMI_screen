#include "ui_events.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "lang.h"
#include "mqtt_app.h"
#include "plant_data.h"
#include "esp_log.h"

static const char *TAG = "ui_evt";

/* ---- Confirmation dialog ---- */

typedef struct {
    const char *mqtt_cmd;   /* command string to publish on YES */
} confirm_ctx_t;

static void confirm_close(lv_event_t *e)
{
    /* Delete the overlay (grandparent of the button) */
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *overlay = (lv_obj_t *)lv_event_get_user_data(e);
    confirm_ctx_t *ctx = (confirm_ctx_t *)lv_obj_get_user_data(overlay);
    if (ctx) lv_free(ctx);
    lv_obj_delete(overlay);
}

static void confirm_yes_cb(lv_event_t *e)
{
    lv_obj_t *overlay = (lv_obj_t *)lv_event_get_user_data(e);
    confirm_ctx_t *ctx = (confirm_ctx_t *)lv_obj_get_user_data(overlay);
    if (ctx) {
        ESP_LOGI(TAG, "CMD: %s (confirmed)", ctx->mqtt_cmd);
        mqtt_publish_mode_cmd(ctx->mqtt_cmd);
        lv_free(ctx);
    }
    lv_obj_delete(overlay);
}

static void show_confirm_dialog(str_id_t message_id, const char *mqtt_cmd)
{
    /* Allocate context for the command string */
    confirm_ctx_t *ctx = lv_malloc(sizeof(confirm_ctx_t));
    ctx->mqtt_cmd = mqtt_cmd;

    /* Semi-transparent overlay on top layer */
    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 1280, 800);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);  /* block clicks through */
    lv_obj_set_user_data(overlay, ctx);

    /* Dialog panel (centered) */
    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 500, 250);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_color(panel, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_pad_all(panel, 24, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, lang_str(STR_CONFIRM_TITLE));
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, UI_FONT_22, 0);
    lv_obj_set_pos(title, 0, 0);

    /* Message */
    lv_obj_t *msg = lv_label_create(panel);
    lv_label_set_text(msg, lang_str(message_id));
    lv_obj_set_style_text_color(msg, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(msg, UI_FONT_22, 0);
    lv_obj_set_width(msg, 450);
    lv_obj_set_pos(msg, 0, 50);

    /* YES button */
    lv_obj_t *btn_yes = lv_button_create(panel);
    lv_obj_set_size(btn_yes, 180, 60);
    lv_obj_set_pos(btn_yes, 40, 150);
    lv_obj_set_style_bg_color(btn_yes, COLOR_BTN_SUCCESS, 0);
    lv_obj_set_style_bg_opa(btn_yes, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_yes, 10, 0);
    lv_obj_t *lbl_yes = lv_label_create(btn_yes);
    lv_label_set_text(lbl_yes, lang_str(STR_BTN_YES));
    lv_obj_set_style_text_color(lbl_yes, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_yes, UI_FONT_22, 0);
    lv_obj_center(lbl_yes);
    lv_obj_add_event_cb(btn_yes, confirm_yes_cb, LV_EVENT_CLICKED, overlay);

    /* NO button */
    lv_obj_t *btn_no = lv_button_create(panel);
    lv_obj_set_size(btn_no, 180, 60);
    lv_obj_set_pos(btn_no, 230, 150);
    lv_obj_set_style_bg_color(btn_no, COLOR_BTN_DANGER, 0);
    lv_obj_set_style_bg_opa(btn_no, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_no, 10, 0);
    lv_obj_t *lbl_no = lv_label_create(btn_no);
    lv_label_set_text(lbl_no, lang_str(STR_BTN_NO));
    lv_obj_set_style_text_color(lbl_no, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_no, UI_FONT_22, 0);
    lv_obj_center(lbl_no);
    lv_obj_add_event_cb(btn_no, confirm_close, LV_EVENT_CLICKED, overlay);
}

/* ---- Mode button callbacks (now show confirmation) ---- */

void ui_evt_start_auto(lv_event_t *e)
{
    (void)e;
    show_confirm_dialog(STR_CONFIRM_START_AUTO, "start_auto");
}

void ui_evt_stop(lv_event_t *e)
{
    (void)e;
    show_confirm_dialog(STR_CONFIRM_STOP, "stop");
}

void ui_evt_set_manual(lv_event_t *e)
{
    (void)e;
    show_confirm_dialog(STR_CONFIRM_MANUAL, "set_manual");
}

void ui_evt_start_washing(lv_event_t *e)
{
    (void)e;
    show_confirm_dialog(STR_CONFIRM_WASHING, "start_washing");
}

void ui_evt_confirm_wash(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "CMD: confirm_wash");
    mqtt_publish_mode_cmd("confirm_wash");
}

void ui_evt_reset_fault(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "CMD: reset_fault");
    mqtt_publish_mode_cmd("reset_fault");
}

void ui_evt_pump_toggle(lv_event_t *e)
{
    /* User data contains bit index (0-4) */
    int bit = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    /* Read current DO mask, toggle bit */
    if (!plant_data_lock(10)) return;
    uint8_t mask = plant_data_get()->do_bits;
    plant_data_unlock();

    if (on) mask |= (1u << bit);
    else    mask &= ~(1u << bit);

    ESP_LOGI(TAG, "CMD: pump mask=0x%02X", mask);
    mqtt_publish_pump_mask(mask);
}

void ui_evt_doser_toggle(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool en = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "CMD: doser enabled=%d", en);
    mqtt_publish_doser_enable(en);
}

void ui_evt_heater_toggle(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "CMD: heater on=%d", on);
    mqtt_publish_heater(on);
}
