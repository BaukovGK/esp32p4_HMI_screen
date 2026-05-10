/**
 * @file ui_events.c
 * @brief Обработчики событий UI -- реакция на нажатия кнопок и переключателей.
 *
 * Модуль реализует:
 *  - Диалог подтверждения (модальный оверлей с кнопками ДА/НЕТ)
 *  - Callback-функции управления режимом работы установки
 *  - Callback-функции ручного управления насосами, дозатором, нагревателем
 *
 * Все команды отправляются на основной контроллер (ESP32-S3) через MQTT.
 */
#include "ui_events.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "lang.h"
#include "mqtt_app.h"
#include "plant_data.h"

#ifndef LVGL_LIVE_PREVIEW
#include "esp_log.h"
#endif

#ifndef LVGL_LIVE_PREVIEW
#include "esp_timer.h"
#endif

#ifdef LVGL_LIVE_PREVIEW
#define ESP_LOGI(tag, ...) ((void)0)
#endif

/** Minimum interval between commands (milliseconds) */
#define CMD_COOLDOWN_MS 1000

static int64_t s_last_cmd_time_us = 0;

/** Check if cooldown has elapsed since last command. Returns true if OK to send. */
static bool check_cooldown(void)
{
#ifndef LVGL_LIVE_PREVIEW
    int64_t now = esp_timer_get_time();
    if ((now - s_last_cmd_time_us) < (CMD_COOLDOWN_MS * 1000LL)) {
        return false;
    }
    s_last_cmd_time_us = now;
#endif
    return true;
}

static bool s_confirm_open = false;

static const char *TAG = "ui_evt";

/* ---- Диалог подтверждения ---- */

/**
 * Контекст диалога подтверждения.
 * Хранит строку MQTT-команды, которая будет отправлена при нажатии "ДА".
 * Выделяется через lv_malloc, освобождается при закрытии диалога.
 */
typedef struct {
    const char *mqtt_cmd;   // MQTT-команда для отправки при подтверждении
} confirm_ctx_t;

/**
 * Callback кнопки "НЕТ" -- закрывает диалог без отправки команды.
 * Освобождает контекст и удаляет оверлей.
 */
static void confirm_close(lv_event_t *e)
{
    lv_obj_t *overlay = (lv_obj_t *)lv_event_get_user_data(e);
    confirm_ctx_t *ctx = (confirm_ctx_t *)lv_obj_get_user_data(overlay);
    if (ctx) lv_free(ctx); // Освобождение контекста диалога
    s_confirm_open = false;
    lv_obj_delete(overlay); // Удаление модального оверлея
}

/**
 * Callback кнопки "ДА" -- отправляет MQTT-команду и закрывает диалог.
 * Публикует команду через mqtt_publish_mode_cmd(), затем очищает ресурсы.
 */
static void confirm_yes_cb(lv_event_t *e)
{
    lv_obj_t *overlay = (lv_obj_t *)lv_event_get_user_data(e);
    confirm_ctx_t *ctx = (confirm_ctx_t *)lv_obj_get_user_data(overlay);
    if (ctx) {
        ESP_LOGI(TAG, "CMD: %s (confirmed)", ctx->mqtt_cmd);
        mqtt_publish_mode_cmd(ctx->mqtt_cmd); // Отправка команды на контроллер
        lv_free(ctx);
    }
    s_confirm_open = false;
    lv_obj_delete(overlay);
}

/**
 * Отображение модального диалога подтверждения.
 *
 * Создаёт полупрозрачный оверлей на верхнем слое LVGL (lv_layer_top),
 * блокирующий нажатия на элементы за ним. Внутри -- панель с заголовком,
 * текстом вопроса и двумя кнопками (ДА / НЕТ).
 *
 * @param message_id идентификатор строки вопроса из таблицы локализации
 * @param mqtt_cmd   строка MQTT-команды для отправки при подтверждении
 */
static void show_confirm_dialog(str_id_t message_id, const char *mqtt_cmd)
{
    if (s_confirm_open) return;
    s_confirm_open = true;

    // Выделение контекста для хранения команды
    confirm_ctx_t *ctx = lv_malloc(sizeof(confirm_ctx_t));
    if (!ctx) {
        s_confirm_open = false;
        return;
    }
    ctx->mqtt_cmd = mqtt_cmd;

    // Полупрозрачный оверлей на весь экран (1280x800)
    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, UI_SCREEN_WIDTH, 800);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);         // 50% прозрачности
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);        // Блокировка кликов сквозь оверлей
    lv_obj_set_user_data(overlay, ctx);

    // Панель диалога (500x250, по центру)
    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 500, 250);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_color(panel, COLOR_ACCENT, 0);  // Рамка акцентного цвета
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_pad_all(panel, 24, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Заголовок диалога
    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, lang_str(STR_CONFIRM_TITLE));
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, UI_FONT_22, 0);
    lv_obj_set_pos(title, 0, 0);

    // Текст вопроса (локализованный)
    lv_obj_t *msg = lv_label_create(panel);
    lv_label_set_text(msg, lang_str(message_id));
    lv_obj_set_style_text_color(msg, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(msg, UI_FONT_22, 0);
    lv_obj_set_width(msg, 450);
    lv_obj_set_pos(msg, 0, 50);

    // Кнопка "ДА" -- зелёная, отправляет команду
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

    // Кнопка "НЕТ" -- красная, закрывает диалог
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

/* ---- Callback-функции управления режимом (через диалог подтверждения) ---- */

/** Запуск автоматического режима -- MQTT-команда "start_auto" */
void ui_evt_start_auto(lv_event_t *e)
{
    if (!check_cooldown()) return;
    (void)e;
    show_confirm_dialog(STR_CONFIRM_START_AUTO, "start_auto");
}

/** Остановка установки — защитное действие, БЕЗ подтверждения.
 * Один тап = команда сразу уходит на контроллер.
 * Cooldown 1 сек оставлен для защиты от случайных double-tap'ов. */
void ui_evt_stop(lv_event_t *e)
{
    (void)e;
    if (!check_cooldown()) return;
    mqtt_publish_mode_cmd("stop");
}

/** Переход в ручной режим -- MQTT-команда "set_manual" */
void ui_evt_set_manual(lv_event_t *e)
{
    if (!check_cooldown()) return;
    (void)e;
    show_confirm_dialog(STR_CONFIRM_MANUAL, "set_manual");
}

/** Запуск промывки мембран -- MQTT-команда "start_washing" */
void ui_evt_start_washing(lv_event_t *e)
{
    if (!check_cooldown()) return;
    (void)e;
    show_confirm_dialog(STR_CONFIRM_WASHING, "start_washing");
}

/** Подтверждение этапа промывки -- отправляется немедленно, без диалога */
void ui_evt_confirm_wash(lv_event_t *e)
{
    if (!check_cooldown()) return;
    (void)e;
    ESP_LOGI(TAG, "CMD: confirm_wash");
    mqtt_publish_mode_cmd("confirm_wash");
}

/** Сброс аварии -- отправляется немедленно, без диалога */
void ui_evt_reset_fault(lv_event_t *e)
{
    if (!check_cooldown()) return;
    (void)e;
    ESP_LOGI(TAG, "CMD: reset_fault");
    mqtt_publish_mode_cmd("reset_fault");
}

/**
 * Переключение насоса в ручном режиме.
 * user_data содержит индекс бита (0..4) в маске дискретных выходов (DO).
 * Читает текущую маску DO, переключает нужный бит и отправляет через MQTT.
 */
void ui_evt_pump_toggle(lv_event_t *e)
{
    if (!check_cooldown()) return;
    int bit = (int)(intptr_t)lv_event_get_user_data(e); // Индекс бита насоса (0..4)
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    // Чтение текущей маски дискретных выходов
    if (!plant_data_lock(10)) return;
    uint8_t mask = plant_data_get()->do_bits;
    plant_data_unlock();

    // Установка или сброс бита в маске
    if (on) mask |= (1u << bit);
    else    mask &= ~(1u << bit);

    ESP_LOGI(TAG, "CMD: pump mask=0x%02X", mask);
    mqtt_publish_pump_mask(mask); // Отправка новой маски на контроллер
}

/** Включение/отключение дозатора антискаланта через MQTT */
void ui_evt_doser_toggle(lv_event_t *e)
{
    if (!check_cooldown()) return;
    lv_obj_t *sw = lv_event_get_target(e);
    bool en = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "CMD: doser enabled=%d", en);
    mqtt_publish_doser_enable(en);
}

/** Включение/отключение нагревателя промывочного раствора через MQTT */
void ui_evt_heater_toggle(lv_event_t *e)
{
    if (!check_cooldown()) return;
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "CMD: heater on=%d", on);
    mqtt_publish_heater(on);
}
