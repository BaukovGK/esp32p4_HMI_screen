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
static esp_mqtt_client_handle_t s_client = NULL;

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to MQTT broker");
        plant_data_set_mqtt_status(true);
        esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_STATUS_ALL, 0);
        esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_ALARMS, 1);
        /* Publish availability */
        esp_mqtt_client_publish(s_client, "ro_hmi/availability", "online", 0, 1, 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from MQTT broker");
        plant_data_set_mqtt_status(false);
        break;

    case MQTT_EVENT_DATA:
        mqtt_handle_incoming(event->topic, event->topic_len,
                             event->data, event->data_len);
        break;

    case MQTT_EVENT_ERROR:
        if (event->error_handle) {
            ESP_LOGE(TAG, "MQTT error type: %d", event->error_handle->error_type);
        }
        break;

    default:
        break;
    }
}

esp_err_t mqtt_client_start(void)
{
    const esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.client_id = CONFIG_MQTT_CLIENT_ID,
        .session.last_will = {
            .topic = "ro_hmi/availability",
            .msg = "offline",
            .msg_len = 7,
            .qos = 1,
            .retain = 1,
        },
        .network.reconnect_timeout_ms = 5000,
        .task.stack_size = 8192,
        .task.priority = 6,
        .buffer.size = 2048,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_client);
}

void mqtt_client_stop(void)
{
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
}

bool mqtt_client_is_connected(void)
{
    if (!plant_data_lock(10)) return false;
    bool c = plant_data_get()->mqtt_connected;
    plant_data_unlock();
    return c;
}

/* ---- Command publishing ---- */

esp_err_t mqtt_publish_mode_cmd(const char *cmd)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_CMD_MODE,
                                      cmd, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_pump_mask(uint8_t mask)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"mask\":%d}", mask);
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_CMD_PUMP,
                                      buf, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_doser_enable(bool enabled)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    const char *msg = enabled ? "{\"enabled\":true}" : "{\"enabled\":false}";
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_CMD_DOSER,
                                      msg, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_heater(bool on)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    const char *msg = on ? "{\"on\":true}" : "{\"on\":false}";
    int ret = esp_mqtt_client_publish(s_client, MQTT_TOPIC_CMD_HEATER,
                                      msg, 0, 1, 0);
    return (ret >= 0) ? ESP_OK : ESP_FAIL;
}

/* ---- Settings publishing ---- */

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
