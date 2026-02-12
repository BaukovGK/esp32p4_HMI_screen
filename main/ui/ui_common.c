/**
 * @file ui_common.c
 * @brief Общие компоненты UI -- панель аварий, навигационная панель, форматирование.
 *
 * Модуль содержит:
 *  - Панель аварий (alarm bar) -- верхняя полоса 40px с режимом, аварией, MQTT, языком
 *  - Навигационную панель (nav bar) -- нижняя полоса 60px с 6 кнопками-вкладками
 *  - Вспомогательные функции форматирования (float с единицами, uptime)
 */
#include "ui_common.h"
#include "ui_theme.h"
#include "lang.h"
#include "alarm_ring.h"
#include "board.h"
#include <stdio.h>
#include <math.h>

/* ========== Панель аварий (Alarm Bar) ========== */

/**
 * Контекст панели аварий -- ссылки на дочерние виджеты для обновления.
 */
typedef struct {
    lv_obj_t *mode_label;   // Метка текущего режима работы установки
    lv_obj_t *alarm_label;  // Метка текста аварии (бегущая строка)
    lv_obj_t *mqtt_label;   // Метка статуса MQTT-соединения
    lv_obj_t *lang_btn;     // Кнопка переключения языка (RU/EN)
} alarm_bar_ctx_t;

static alarm_bar_ctx_t s_abar; // Единственный экземпляр контекста панели аварий

/**
 * Callback кнопки переключения языка интерфейса (RU <-> EN).
 * Переключает язык и обновляет текст на кнопке.
 */
static void lang_btn_cb(lv_event_t *e)
{
    (void)e;
    lang_set(lang_get() == LANG_RU ? LANG_EN : LANG_RU);
    lv_obj_t *label = lv_obj_get_child(s_abar.lang_btn, 0);
    lv_label_set_text(label, lang_get() == LANG_RU ? "RU" : "EN");
}

/**
 * Создание панели аварий.
 * Располагается в верхней части экрана, содержит 4 элемента в flex-строке:
 *  1. Режим работы (250px) -- цвет зависит от состояния установки
 *  2. Текст аварии (flex-grow) -- бегущая строка с круговой прокруткой
 *  3. Статус MQTT (80px) -- "MQTT:OK" зелёный / "MQTT:--" красный
 *  4. Кнопка языка (50x30) -- переключает RU/EN
 */
lv_obj_t *ui_alarm_bar_create(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, BOARD_DISP_H_RES, 40);  // Полная ширина экрана, высота 40px
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 4, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);  // Горизонтальный flex-контейнер
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // Метка режима работы (IDLE / AUTO / WASHING / MANUAL / FAULT)
    s_abar.mode_label = lv_label_create(bar);
    lv_label_set_text(s_abar.mode_label, "---");
    lv_obj_set_style_text_color(s_abar.mode_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_width(s_abar.mode_label, 250);

    // Метка текста аварии -- бегущая строка при длинном тексте
    s_abar.alarm_label = lv_label_create(bar);
    lv_label_set_text(s_abar.alarm_label, "");
    lv_obj_set_style_text_color(s_abar.alarm_label, COLOR_ALARM_WARNING, 0);
    lv_obj_set_flex_grow(s_abar.alarm_label, 1);  // Занимает оставшееся место
    lv_label_set_long_mode(s_abar.alarm_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // Индикатор статуса MQTT-подключения к контроллеру
    s_abar.mqtt_label = lv_label_create(bar);
    lv_label_set_text(s_abar.mqtt_label, "MQTT:?");
    lv_obj_set_style_text_color(s_abar.mqtt_label, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_width(s_abar.mqtt_label, 80);

    // Кнопка переключения языка интерфейса
    s_abar.lang_btn = lv_button_create(bar);
    lv_obj_set_size(s_abar.lang_btn, 50, 30);
    lv_obj_set_style_bg_color(s_abar.lang_btn, COLOR_BG_WIDGET, 0);
    lv_obj_add_event_cb(s_abar.lang_btn, lang_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lang_lbl = lv_label_create(s_abar.lang_btn);
    lv_label_set_text(lang_lbl, lang_get() == LANG_RU ? "RU" : "EN");
    lv_obj_center(lang_lbl);

    return bar;
}

/**
 * Обновление панели аварий актуальными данными установки.
 *
 * Обновляет три элемента:
 *  1. Режим -- основной режим + подрежим (авто-подрежимы, этапы промывки)
 *  2. Авария -- код наиболее критичной активной аварии из кольцевого буфера
 *  3. MQTT -- статус подключения к брокеру
 */
void ui_alarm_bar_update(lv_obj_t *bar, const plant_data_t *data)
{
    (void)bar;

    // Обновление метки режима с подрежимами (АВТО / подэтап, ПРОМЫВКА / подэтап)
    const char *mode_str = lang_str(STR_MODE_IDLE + (int)data->state);
    char mode_buf[64];
    if (data->state == PLANT_STATE_AUTO && data->auto_sub != AUTO_SUB_NONE) {
        snprintf(mode_buf, sizeof(mode_buf), "%s / %s", mode_str,
                 lang_str(STR_AUTO_STARTING_PUMP1 + (int)data->auto_sub - 1));
        lv_label_set_text(s_abar.mode_label, mode_buf);
    } else if (data->state == PLANT_STATE_WASHING && data->wash_sub != WASH_SUB_NONE) {
        snprintf(mode_buf, sizeof(mode_buf), "%s / %s", mode_str,
                 lang_str(STR_WASH_WAIT_HEAT + (int)data->wash_sub - 1));
        lv_label_set_text(s_abar.mode_label, mode_buf);
    } else {
        lv_label_set_text(s_abar.mode_label, mode_str);
    }
    // Цвет метки зависит от состояния: серый/зелёный/синий/жёлтый/красный
    lv_obj_set_style_text_color(s_abar.mode_label, ui_state_color(data->state), 0);

    // Обновление метки аварий -- показывается наиболее критичная активная авария
    alarm_entry_t worst;
    if (alarm_ring_get_active(&worst, 1) > 0) {
        lv_label_set_text(s_abar.alarm_label, worst.code);
        lv_obj_set_style_text_color(s_abar.alarm_label,
                                    ui_alarm_cat_color(worst.cat), 0);
    } else {
        lv_label_set_text(s_abar.alarm_label, ""); // Нет активных аварий
    }

    // Обновление статуса MQTT -- зелёный при подключении, красный при обрыве
    lv_label_set_text(s_abar.mqtt_label,
                      data->mqtt_connected ? "MQTT:OK" : "MQTT:--");
    lv_obj_set_style_text_color(s_abar.mqtt_label,
        data->mqtt_connected ? COLOR_STATE_AUTO : COLOR_STATE_FAULT, 0);
}

/* ========== Навигационная панель (Navigation Bar) ========== */

/**
 * Контекст навигационной панели -- кнопки, callback, активная вкладка.
 */
typedef struct {
    lv_obj_t *btns[6]; // Массив из 6 кнопок-вкладок
    nav_cb_t  cb;      // Callback при нажатии на вкладку
    int       active;  // Индекс текущей активной вкладки
} nav_bar_ctx_t;

static nav_bar_ctx_t s_nav; // Единственный экземпляр контекста навигации

/**
 * Callback нажатия кнопки навигации.
 * Извлекает индекс экрана из user_data и вызывает зарегистрированный callback.
 */
static void nav_btn_cb(lv_event_t *e)
{
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_nav.cb) s_nav.cb(id);
}

// Локализованные названия вкладок навигации
static const str_id_t nav_labels[6] = {
    STR_NAV_MNEMONIC, STR_NAV_PARAMETERS, STR_NAV_WASHING,
    STR_NAV_ALARMS, STR_NAV_SETTINGS, STR_NAV_DIAGNOSTICS,
};

/**
 * Создание навигационной панели с 6 кнопками-вкладками.
 * Располагается в нижней части экрана (60px). Каждая кнопка 190x50,
 * в состоянии CHECKED подсвечивается акцентным цветом.
 */
lv_obj_t *ui_nav_bar_create(lv_obj_t *parent, nav_cb_t on_select)
{
    s_nav.cb = on_select;
    s_nav.active = 0;

    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, BOARD_DISP_H_RES, 60); // Полная ширина, высота 60px
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 2, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW); // Горизонтальный flex
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // Создание 6 кнопок навигации
    for (int i = 0; i < 6; i++) {
        lv_obj_t *btn = lv_button_create(bar);
        lv_obj_set_size(btn, 190, 50);
        lv_obj_set_style_bg_color(btn, COLOR_BG_WIDGET, 0);             // Обычный цвет
        lv_obj_set_style_bg_color(btn, COLOR_ACCENT, LV_STATE_CHECKED); // Цвет активной
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);                    // Поддержка состояния CHECKED
        lv_obj_add_event_cb(btn, nav_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, lang_str(nav_labels[i])); // Локализованное название
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_PRIMARY, 0);
        lv_obj_center(lbl);

        s_nav.btns[i] = btn;
    }

    // Первая вкладка активна по умолчанию
    lv_obj_add_state(s_nav.btns[0], LV_STATE_CHECKED);

    return bar;
}

/**
 * Установка активной вкладки навигации.
 * Снимает CHECKED со всех кнопок, устанавливает на указанную.
 */
void ui_nav_bar_set_active(lv_obj_t *bar, int screen_id)
{
    (void)bar;
    if (screen_id < 0 || screen_id >= 6) return;

    for (int i = 0; i < 6; i++) {
        if (i == screen_id) {
            lv_obj_add_state(s_nav.btns[i], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_nav.btns[i], LV_STATE_CHECKED);
        }
    }
    s_nav.active = screen_id;
}

/* ========== Вспомогательные функции форматирования ========== */

/**
 * Форматирование вещественного числа с единицей измерения.
 * При NaN выводит "--- <unit>" для индикации неисправности датчика.
 */
void ui_fmt_float(char *buf, size_t len, float val, int decimals, const char *unit)
{
    if (isnan(val)) {
        snprintf(buf, len, "--- %s", unit ? unit : ""); // Обрыв датчика
    } else {
        snprintf(buf, len, "%.*f %s", decimals, val, unit ? unit : "");
    }
}

/**
 * Форматирование времени работы (uptime).
 * При наличии дней: "Xd HH:MM:SS", иначе: "HH:MM:SS".
 */
void ui_fmt_uptime(char *buf, size_t len, int64_t seconds)
{
    int d = (int)(seconds / 86400);        // Дни
    int h = (int)((seconds % 86400) / 3600); // Часы
    int m = (int)((seconds % 3600) / 60);    // Минуты
    int s = (int)(seconds % 60);             // Секунды
    if (d > 0) {
        snprintf(buf, len, "%dd %02d:%02d:%02d", d, h, m, s);
    } else {
        snprintf(buf, len, "%02d:%02d:%02d", h, m, s);
    }
}
