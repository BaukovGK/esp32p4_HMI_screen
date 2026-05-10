/**
 * @file mqtt_app.c
 * @brief MQTT-клиент для HMI-дисплея установки обратного осмоса.
 *
 * Управляет подключением к MQTT-брокеру, подпиской на топики статуса
 * и аварий, публикацией команд управления и настроек.
 * Использует ESP-IDF MQTT client API. При подключении подписывается
 * на ro_plant/status/# (QoS 0) и ro_plant/alarms (QoS 1).
 * Публикует Last Will "offline" на топик ro_hmi/availability.
 */
#ifndef LVGL_LIVE_PREVIEW
#include "mqtt_app.h"
#include "mqtt_topics.h"
#include "mqtt_parser.h"
#include "plant_data.h"
#include "mqtt_client.h"  /* ESP-IDF MQTT client API */
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "mqtt_app";

/** Дескриптор MQTT-клиента ESP-IDF (NULL если не инициализирован) */
static esp_mqtt_client_handle_t s_client = NULL;

/* ---- Состояние подписок (H1: диагностика ACL-deny на брокере) ---- */
/** Количество subscribe()-вызовов в CONNECTED, ещё не подтверждённых SUBSCRIBED. */
static int s_pending_subs = 0;
/** Сколько подписок выпущено в последнем CONNECTED (для лога). */
static int s_issued_subs  = 0;

/* ---- Backoff-аккаунтинг для reconnect (H8) ----
 * ESP-IDF MQTT в IDF 5.4 сам реконнектит с фиксированным
 * .network.reconnect_timeout_ms (см. mqtt_client.h:332). Встроенного
 * экспоненциального backoff в API нет, поэтому здесь ведём только
 * счётчик попыток для логирования; собственно интервал — на стороне IDF.
 */
static int s_reconnect_attempts = 0;

/* ---- Сборка фрагментированных MQTT-сообщений (H2) ----
 * При payload > buffer.size ESP-IDF MQTT доставляет данные несколькими
 * MQTT_EVENT_DATA подряд: первый с current_data_offset==0, последующие
 * с накапливающимся offset. Топик передаётся только в первом фрагменте.
 * Раньше всё это молча отбрасывалось; теперь склеиваем в s_frag_buf
 * и в момент поступления последнего фрагмента отдаём целиком парсеру.
 */
#define MQTT_FRAG_BUF_SIZE   8192
#define MQTT_FRAG_TOPIC_SIZE 128
static char     s_frag_buf[MQTT_FRAG_BUF_SIZE];
static char     s_frag_topic[MQTT_FRAG_TOPIC_SIZE];
static int      s_frag_topic_len = 0;
static int      s_frag_offset    = 0;        /* сколько байт payload уже скопировано */
static int      s_frag_total     = 0;        /* ожидаемый total_data_len первого фрагмента */
static bool     s_frag_active    = false;    /* идёт сборка многочастного сообщения */
static uint32_t s_dropped_fragments = 0;     /* статистика: сколько пакетов отброшено из-за переполнения */

static void mqtt_frag_reset(void)
{
    s_frag_active    = false;
    s_frag_offset    = 0;
    s_frag_total     = 0;
    s_frag_topic_len = 0;
}

/**
 * Обработчик событий MQTT-клиента.
 *
 * Обрабатывает следующие события:
 * - MQTT_EVENT_CONNECTED    -- подписка на топики статуса и аварий,
 *                              публикация "online" на ro_hmi/availability
 * - MQTT_EVENT_DISCONNECTED -- обновление статуса подключения в plant_data
 * - MQTT_EVENT_DATA         -- маршрутизация входящих сообщений через mqtt_handle_incoming()
 * - MQTT_EVENT_ERROR        -- логирование типа ошибки
 */
static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: {
        ESP_LOGI(TAG, "Connected to MQTT broker (after %d reconnect attempt%s)",
                 s_reconnect_attempts, s_reconnect_attempts == 1 ? "" : "s");
        s_reconnect_attempts = 0;          /* H8: сбросить счётчик при успешном connect */
        mqtt_frag_reset();                 /* H2: на новом коннекте не доедаем старые фрагменты */
        s_pending_subs = 0;                /* H1: новый коннект — заново отслеживаем подписки */
        s_issued_subs  = 0;
        // Обновляем статус подключения в общей структуре данных
        plant_data_set_mqtt_status(true);

        int msg_id;
        // Подписка на все топики статуса установки (QoS 0)
        msg_id = esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_STATUS_ALL, 0);
        if (msg_id < 0) ESP_LOGE(TAG, "Failed to subscribe to %s", MQTT_TOPIC_STATUS_ALL);
        else { s_pending_subs++; s_issued_subs++; }
        // Подписка на аварийные сообщения (QoS 1 для гарантии доставки)
        msg_id = esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_ALARMS, 1);
        if (msg_id < 0) ESP_LOGE(TAG, "Failed to subscribe to %s", MQTT_TOPIC_ALARMS);
        else { s_pending_subs++; s_issued_subs++; }
        // Подписка на availability контроллера (retained, QoS 1)
        msg_id = esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_AVAILABILITY, 1);
        if (msg_id < 0) ESP_LOGE(TAG, "Failed to subscribe to %s", MQTT_TOPIC_AVAILABILITY);
        else { s_pending_subs++; s_issued_subs++; }
        /* Публикация доступности HMI (retained) */
        msg_id = esp_mqtt_client_publish(s_client, MQTT_TOPIC_HMI_AVAILABILITY, "online", 0, 1, 1);
        if (msg_id < 0) ESP_LOGE(TAG, "Failed to publish availability");
        ESP_LOGI(TAG, "issued %d subscribe requests, awaiting SUBACK", s_issued_subs);
        break;
    }

    case MQTT_EVENT_DISCONNECTED:
        s_reconnect_attempts++;            /* H8: считаем «попыток было N» — сам реконнект делает IDF */
        ESP_LOGW(TAG, "Disconnected from MQTT broker (reconnect attempt %d, IDF retries every reconnect_timeout_ms)",
                 s_reconnect_attempts);
        mqtt_frag_reset();                 /* H2: незавершённая сборка более не валидна */
        // Сброс статуса подключения
        plant_data_set_mqtt_status(false);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        if (s_pending_subs > 0) s_pending_subs--;
        ESP_LOGI(TAG, "subscribed: msg_id=%d (pending=%d/%d)",
                 event->msg_id, s_pending_subs, s_issued_subs);
        if (s_pending_subs == 0 && s_issued_subs > 0) {
            ESP_LOGI(TAG, "all %d subscriptions confirmed by broker", s_issued_subs);
        }
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGW(TAG, "unsubscribed: msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA: {
        /* H2: сборка фрагментов. ESP-IDF MQTT доставляет topic только в первом
         * фрагменте; в последующих event->topic_len == 0. Разруливаем оба случая. */
        const bool is_first = (event->current_data_offset == 0);
        const bool fits_in_one = is_first && (event->data_len == event->total_data_len);

        if (fits_in_one) {
            /* Быстрый путь: сообщение поместилось в один пакет — без буферизации. */
            mqtt_handle_incoming(event->topic, event->topic_len,
                                 event->data, event->data_len);
            break;
        }

        if (is_first) {
            /* Начало нового многочастного сообщения. */
            if (event->total_data_len > MQTT_FRAG_BUF_SIZE) {
                ESP_LOGW(TAG, "Fragmented payload too big (%d > %d), dropping",
                         event->total_data_len, MQTT_FRAG_BUF_SIZE);
                s_dropped_fragments++;
                mqtt_frag_reset();
                break;
            }
            if ((size_t)event->topic_len + 1 > sizeof(s_frag_topic)) {
                ESP_LOGW(TAG, "Fragmented topic too long (%d), dropping", event->topic_len);
                s_dropped_fragments++;
                mqtt_frag_reset();
                break;
            }
            memcpy(s_frag_topic, event->topic, (size_t)event->topic_len);
            s_frag_topic[event->topic_len] = '\0';
            s_frag_topic_len = event->topic_len;
            memcpy(s_frag_buf, event->data, (size_t)event->data_len);
            s_frag_offset = event->data_len;
            s_frag_total  = event->total_data_len;
            s_frag_active = true;
        } else {
            /* Продолжение. Должно быть после валидного начала. */
            if (!s_frag_active) {
                ESP_LOGW(TAG, "fragment continuation without start (offset=%d), dropping",
                         event->current_data_offset);
                s_dropped_fragments++;
                break;
            }
            if (event->current_data_offset != s_frag_offset ||
                s_frag_offset + event->data_len > MQTT_FRAG_BUF_SIZE ||
                s_frag_offset + event->data_len > s_frag_total) {
                ESP_LOGW(TAG, "fragment overflow/misorder (have=%d got_off=%d add=%d total=%d), dropping",
                         s_frag_offset, event->current_data_offset,
                         event->data_len, s_frag_total);
                s_dropped_fragments++;
                mqtt_frag_reset();
                break;
            }
            memcpy(s_frag_buf + s_frag_offset, event->data, (size_t)event->data_len);
            s_frag_offset += event->data_len;
        }

        /* Полное сообщение собрано — отдать наверх и сбросить состояние. */
        if (s_frag_active && s_frag_offset == s_frag_total) {
            mqtt_handle_incoming(s_frag_topic, s_frag_topic_len,
                                 s_frag_buf, s_frag_offset);
            mqtt_frag_reset();
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        if (event->error_handle) {
            ESP_LOGE(TAG, "MQTT error type: %d", event->error_handle->error_type);
        } else {
            ESP_LOGE(TAG, "MQTT error (no handle)");
        }
        break;

    default:
        break;
    }
}

/**
 * Инициализация и запуск MQTT-клиента.
 *
 * Конфигурация клиента:
 * - URI брокера: из CONFIG_MQTT_BROKER_URI (по умолчанию mqtt://192.168.1.1:1883)
 * - Client ID: из CONFIG_MQTT_CLIENT_ID
 * - Last Will: "offline" на ro_hmi/availability (QoS 1, retained)
 * - Автопереподключение каждые 5 секунд
 * - Размер стека задачи: 8192, приоритет: 6
 * - Размер буфера: 2048 байт
 *
 * @return ESP_OK при успехе, ESP_FAIL если инициализация не удалась
 */
esp_err_t mqtt_client_start(void)
{
    const esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.client_id = CONFIG_MQTT_CLIENT_ID,
        .session.last_will = {
            .topic = MQTT_TOPIC_HMI_AVAILABILITY,  // Топик Last Will Testament
            .msg = "offline",                  // Сообщение при потере связи
            .msg_len = 7,
            .qos = 1,                          // Гарантированная доставка
            .retain = 1,                       // Сохранение на брокере
        },
        .network.reconnect_timeout_ms = 5000,  // Переподключение каждые 5 сек
        .task.stack_size = 8192,                // Размер стека задачи MQTT
        .task.priority = 6,                     // Приоритет задачи MQTT
        .buffer.size = 4096,                    // Размер буфера приема/передачи (было 2048, увеличено до 4096 для diagnostics-payload с modbus)
    };

    // Инициализация MQTT-клиента
    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }

    // Регистрация обработчика всех событий MQTT
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_client);
}

/**
 * Остановка и уничтожение MQTT-клиента.
 *
 * Корректно останавливает клиент, освобождает ресурсы и обнуляет дескриптор.
 */
void mqtt_client_stop(void)
{
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
}

/**
 * Проверка статуса подключения MQTT-клиента.
 *
 * Потокобезопасное чтение поля mqtt_connected из plant_data
 * с захватом мьютекса на 10 мс.
 *
 * @return true если клиент подключен к брокеру
 */
bool mqtt_client_is_connected(void)
{
    if (!plant_data_lock(10)) return false;
    bool c = plant_data_get()->mqtt_connected;
    plant_data_unlock();
    return c;
}

/* ---- Публикация команд управления ---- */

/**
 * Публикация команды смены режима работы установки.
 *
 * Отправляет строковую команду (например, "AUTO", "IDLE", "MANUAL")
 * на топик ro_plant/command/mode (QoS 1).
 *
 * @param cmd Строка команды режима
 * @return ESP_OK при успехе, ESP_ERR_INVALID_STATE если клиент не инициализирован
 */
esp_err_t mqtt_publish_mode_cmd(const char *cmd)
{
    if (!s_client || !cmd) return ESP_ERR_INVALID_STATE;
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_CMD_MODE,
                                      cmd, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * Публикация битовой маски управления насосами.
 *
 * Отправляет JSON {"mask": N} на топик ro_plant/command/pump (QoS 1).
 * Каждый бит маски соответствует одному насосу.
 *
 * @param mask Битовая маска включения насосов
 * @return ESP_OK при успехе, ESP_ERR_INVALID_STATE если клиент не инициализирован
 */
esp_err_t mqtt_publish_pump_mask(uint8_t mask)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"mask\":%d}", mask);
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_CMD_PUMP,
                                      buf, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * Публикация команды включения/выключения дозатора.
 *
 * Отправляет JSON {"enabled": true/false} на топик ro_plant/command/doser (QoS 1).
 *
 * @param enabled true -- включить дозатор, false -- выключить
 * @return ESP_OK при успехе, ESP_ERR_INVALID_STATE если клиент не инициализирован
 */
esp_err_t mqtt_publish_doser_enable(bool enabled)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    const char *msg = enabled ? "{\"enabled\":true}" : "{\"enabled\":false}";
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_CMD_DOSER,
                                      msg, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * Публикация команды включения/выключения нагревателя.
 *
 * Отправляет JSON {"on": true/false} на топик ro_plant/command/heater (QoS 1).
 *
 * @param on true -- включить нагреватель, false -- выключить
 * @return ESP_OK при успехе, ESP_ERR_INVALID_STATE если клиент не инициализирован
 */
esp_err_t mqtt_publish_heater(bool on)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    const char *msg = on ? "{\"on\":true}" : "{\"on\":false}";
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_CMD_HEATER,
                                      msg, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

/* ---- Публикация настроек (уставок) ---- */

/**
 * Публикация настроек давления.
 *
 * Отправляет JSON с уставками давления на топик ro_plant/settings/pressure (QoS 1).
 * Поля: p1_max, p3_max, p4_max, filter_dp_warn (все в барах).
 *
 * @param s Указатель на структуру настроек давления
 * @return ESP_OK при успехе, ESP_ERR_INVALID_STATE если клиент не инициализирован
 */
esp_err_t mqtt_publish_settings_pressure(const settings_pressure_t *s)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"p1_max\":%.1f,\"p3_max\":%.1f,\"p4_max\":%.1f,\"filter_dp_warn\":%.1f}",
             (double)s->p1_max, (double)s->p3_max, (double)s->p4_max,
             (double)s->filter_dp_warn);
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_SET_PRESSURE, buf, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * Публикация настроек дозатора.
 *
 * Отправляет JSON с временами работы на топик ro_plant/settings/doser (QoS 1).
 * Поля: run_time_min (время работы), cycle_time_min (период цикла).
 *
 * @param s Указатель на структуру настроек дозатора
 * @return ESP_OK при успехе, ESP_ERR_INVALID_STATE если клиент не инициализирован
 */
esp_err_t mqtt_publish_settings_doser(const settings_doser_t *s)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    char buf[64];
    snprintf(buf, sizeof(buf),
             "{\"run_time_min\":%d,\"cycle_time_min\":%d}",
             s->run_time_min, s->cycle_time_min);
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_SET_DOSER, buf, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * Публикация настроек химической промывки мембран.
 *
 * Отправляет JSON с параметрами промывки на топик ro_plant/settings/washing (QoS 1).
 * Поля: target_temp_C, max_temp_C, t_overshoot_C, hysteresis_C (температуры),
 *       heat_timeout_min, supply_time_min, drain_time_min (времена в минутах).
 *
 * @param s Указатель на структуру настроек промывки
 * @return ESP_OK при успехе, ESP_ERR_INVALID_STATE если клиент не инициализирован
 */
esp_err_t mqtt_publish_settings_washing(const settings_washing_t *s)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"target_temp_C\":%.1f,\"max_temp_C\":%.1f,\"t_overshoot_C\":%.1f,"
             "\"hysteresis_C\":%.1f,\"heat_timeout_min\":%d,"
             "\"supply_time_min\":%d,\"drain_time_min\":%d}",
             (double)s->target_temp_C, (double)s->max_temp_C,
             (double)s->t_overshoot_C, (double)s->hysteresis_C,
             s->heat_timeout_min,
             s->supply_time_min, s->drain_time_min);
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_SET_WASHING, buf, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

/**
 * Публикация настроек таймаутов насосов.
 *
 * Отправляет JSON с таймаутами на топик ro_plant/settings/timeouts (QoS 1).
 * Поля: pump_confirm_ms (таймаут подтверждения), pump_ramp_ms (время разгона).
 *
 * @param s Указатель на структуру настроек таймаутов
 * @return ESP_OK при успехе, ESP_ERR_INVALID_STATE если клиент не инициализирован
 */
esp_err_t mqtt_publish_settings_timeouts(const settings_timeouts_t *s)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    char buf[64];
    snprintf(buf, sizeof(buf),
             "{\"pump_confirm_ms\":%d,\"pump_ramp_ms\":%d}",
             s->pump_confirm_ms, s->pump_ramp_ms);
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_SET_TIMEOUTS, buf, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}
#endif /* !LVGL_LIVE_PREVIEW */
