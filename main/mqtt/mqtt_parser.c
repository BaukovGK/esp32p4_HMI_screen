#include "mqtt_parser.h"
#include "mqtt_topics.h"
#include "plant_data.h"
#include "alarm_ring.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "mqtt_parse";

/* ---- Helpers ---- */

static float json_get_float(const cJSON *obj, const char *key, float def)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) return def;
    if (cJSON_IsNull(item)) return NAN;
    if (cJSON_IsNumber(item)) return (float)item->valuedouble;
    return def;
}

static bool json_get_bool(const cJSON *obj, const char *key, bool def)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) return def;
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
    return def;
}

static int json_get_int(const cJSON *obj, const char *key, int def)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    return item->valueint;
}

static const char *json_get_string(const cJSON *obj, const char *key, const char *def)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsString(item)) return def;
    return item->valuestring;
}

/* ---- State parser ---- */

static plant_state_t parse_state_enum(const char *s)
{
    if (!s) return PLANT_STATE_UNKNOWN;
    if (strcmp(s, "IDLE") == 0)    return PLANT_STATE_IDLE;
    if (strcmp(s, "AUTO") == 0)    return PLANT_STATE_AUTO;
    if (strcmp(s, "WASHING") == 0) return PLANT_STATE_WASHING;
    if (strcmp(s, "MANUAL") == 0)  return PLANT_STATE_MANUAL;
    if (strcmp(s, "FAULT") == 0)   return PLANT_STATE_FAULT;
    return PLANT_STATE_UNKNOWN;
}

static auto_sub_state_t parse_auto_sub(const char *s)
{
    if (!s) return AUTO_SUB_NONE;
    if (strcmp(s, "STARTING_PUMP1") == 0)  return AUTO_SUB_STARTING_PUMP1;
    if (strcmp(s, "RAMP") == 0)            return AUTO_SUB_RAMP;
    if (strcmp(s, "STARTING_PUMP2") == 0)  return AUTO_SUB_STARTING_PUMP2;
    if (strcmp(s, "FILLING_INTERM") == 0)  return AUTO_SUB_FILLING_INTERM;
    if (strcmp(s, "STARTING_PUMP3") == 0)  return AUTO_SUB_STARTING_PUMP3;
    if (strcmp(s, "RUNNING") == 0)         return AUTO_SUB_RUNNING;
    if (strcmp(s, "STOPPING") == 0)        return AUTO_SUB_STOPPING;
    return AUTO_SUB_NONE;
}

static wash_sub_state_t parse_wash_sub(const char *s)
{
    if (!s) return WASH_SUB_NONE;
    if (strcmp(s, "WAIT_HEAT") == 0)    return WASH_SUB_WAIT_HEAT;
    if (strcmp(s, "HEATING") == 0)      return WASH_SUB_HEATING;
    if (strcmp(s, "WAIT_SUPPLY") == 0)  return WASH_SUB_WAIT_SUPPLY;
    if (strcmp(s, "SUPPLY") == 0)       return WASH_SUB_SUPPLY;
    if (strcmp(s, "WAIT_DRAIN") == 0)   return WASH_SUB_WAIT_DRAIN;
    if (strcmp(s, "DRAIN") == 0)        return WASH_SUB_DRAIN;
    if (strcmp(s, "DONE") == 0)         return WASH_SUB_DONE;
    return WASH_SUB_NONE;
}

static void parse_state(const cJSON *json)
{
    plant_state_t st = parse_state_enum(json_get_string(json, "state", NULL));
    auto_sub_state_t asub = parse_auto_sub(json_get_string(json, "auto_sub", NULL));
    wash_sub_state_t wsub = parse_wash_sub(json_get_string(json, "wash_sub", NULL));
    uint32_t ff = (uint32_t)json_get_int(json, "fault_flags", 0);
    plant_data_set_state(st, asub, wsub, ff);
}

/* ---- I/O parser ---- */

static void parse_io(const cJSON *json)
{
    uint8_t di = (uint8_t)json_get_int(json, "di", 0);
    uint8_t do_bits = (uint8_t)json_get_int(json, "do", 0);
    plant_data_set_io(di, do_bits);
}

/* ---- Analog parser ---- */

static void parse_analog(const cJSON *json, const char *name)
{
    float val = json_get_float(json, "value", NAN);
    bool fault = json_get_bool(json, "fault", false);

    if (strcmp(name, "P1") == 0)      plant_data_set_pressure(0, val, fault);
    else if (strcmp(name, "P2") == 0) plant_data_set_pressure(1, val, fault);
    else if (strcmp(name, "P3") == 0) plant_data_set_pressure(2, val, fault);
    else if (strcmp(name, "P4") == 0) plant_data_set_pressure(3, val, fault);
    else if (strcmp(name, "T") == 0)  plant_data_set_temperature(val, fault);
}

/* ---- Flow parser ---- */

static void parse_flow(const cJSON *json, const char *name)
{
    float f = json_get_float(json, "flow", NAN);
    float v = json_get_float(json, "volume", NAN);
    bool ok = json_get_bool(json, "ok", false);

    int idx = -1;
    if (strcmp(name, "Q1") == 0)      idx = 0;
    else if (strcmp(name, "Q2") == 0) idx = 1;
    else if (strcmp(name, "Q3") == 0) idx = 2;
    else if (strcmp(name, "Q4") == 0) idx = 3;

    if (idx >= 0) plant_data_set_flow(idx, f, v, ok);
}

/* ---- Conductivity parser ---- */

static void parse_conductivity(const cJSON *json, const char *name)
{
    float c = json_get_float(json, "conductivity", NAN);
    float t = json_get_float(json, "temperature", NAN);
    bool ok = json_get_bool(json, "ok", false);

    int idx = -1;
    if (strcmp(name, "s1") == 0)      idx = 0;
    else if (strcmp(name, "s2") == 0) idx = 1;
    else if (strcmp(name, "s3") == 0) idx = 2;

    if (idx >= 0) plant_data_set_conductivity(idx, c, t, ok);
}

/* ---- Telemetry parser ---- */

static void parse_telemetry(const cJSON *json)
{
    telemetry_t tel = {
        .filter_dp    = json_get_float(json, "filter_dp", NAN),
        .stage1_feed  = json_get_float(json, "stage1_feed", NAN),
        .recovery2    = json_get_float(json, "recovery2", NAN),
        .recovery_sys = json_get_float(json, "recovery_sys", NAN),
        .sel1         = json_get_float(json, "sel1", NAN),
        .sel2         = json_get_float(json, "sel2", NAN),
    };
    plant_data_set_telemetry(&tel);
}

/* ---- Doser parser ---- */

static void parse_doser(const cJSON *json)
{
    const char *st = json_get_string(json, "state", "OFF");
    doser_state_t ds = DOSER_STATE_OFF;
    if (strcmp(st, "RUNNING") == 0) ds = DOSER_STATE_RUNNING;
    else if (strcmp(st, "PAUSE") == 0) ds = DOSER_STATE_PAUSE;

    bool en = json_get_bool(json, "enabled", false);
    plant_data_set_doser(ds, en);
}

/* ---- Interlocks parser ---- */

static void parse_interlocks(const cJSON *json)
{
    uint32_t flags = (uint32_t)json_get_int(json, "flags", 0);
    bool estop = json_get_bool(json, "estop", false);
    bool fw = json_get_bool(json, "filter_warn", false);
    plant_data_set_interlocks(flags, estop, fw);
}

/* ---- Diagnostics parser ---- */

static void parse_diagnostics(const cJSON *json)
{
    diagnostics_t diag = {0};
    diag.heap_free = (uint32_t)json_get_int(json, "heap_free", 0);
    diag.heap_min  = (uint32_t)json_get_int(json, "heap_min", 0);
    diag.uptime_s  = (int64_t)json_get_int(json, "uptime_s", 0);
    diag.wdt_stale = (uint32_t)json_get_int(json, "wdt_stale", 0);

    cJSON *stack = cJSON_GetObjectItemCaseSensitive(json, "stack");
    if (stack) {
        diag.stack_modbus   = (uint32_t)json_get_int(stack, "modbus", 0);
        diag.stack_io       = (uint32_t)json_get_int(stack, "io", 0);
        diag.stack_process  = (uint32_t)json_get_int(stack, "process", 0);
        diag.stack_watchdog = (uint32_t)json_get_int(stack, "watchdog", 0);
        diag.stack_mqtt     = (uint32_t)json_get_int(stack, "mqtt", 0);
    }

    cJSON *modbus = cJSON_GetObjectItemCaseSensitive(json, "modbus");
    if (modbus) {
        cJSON *errors = cJSON_GetObjectItemCaseSensitive(modbus, "errors");
        cJSON *online = cJSON_GetObjectItemCaseSensitive(modbus, "online");
        for (int i = 0; i < 4; i++) {
            if (errors && cJSON_IsArray(errors)) {
                cJSON *e = cJSON_GetArrayItem(errors, i);
                if (e) diag.modbus_errors[i] = (uint32_t)e->valueint;
            }
            if (online && cJSON_IsArray(online)) {
                cJSON *o = cJSON_GetArrayItem(online, i);
                if (o) diag.modbus_online[i] = cJSON_IsTrue(o);
            }
        }
    }

    plant_data_set_diagnostics(&diag);
}

/* ---- Alarm parser ---- */

static alarm_category_t parse_alarm_cat(const char *s)
{
    if (!s) return ALARM_CAT_INFO;
    if (strcmp(s, "CRITICAL") == 0) return ALARM_CAT_CRITICAL;
    if (strcmp(s, "ALARM") == 0)    return ALARM_CAT_ALARM;
    if (strcmp(s, "WARNING") == 0)  return ALARM_CAT_WARNING;
    return ALARM_CAT_INFO;
}

static void parse_alarm(const cJSON *json)
{
    alarm_entry_t entry = {0};
    entry.id = (uint32_t)json_get_int(json, "id", 0);
    entry.ts = (int64_t)json_get_int(json, "ts", 0);
    entry.cat = parse_alarm_cat(json_get_string(json, "cat", "INFO"));
    entry.value = json_get_float(json, "value", 0);
    entry.active = json_get_bool(json, "active", true);

    const char *code = json_get_string(json, "code", "UNKNOWN");
    strncpy(entry.code, code, sizeof(entry.code) - 1);

    alarm_ring_push(&entry);

    /* Also set dirty flag in plant_data */
    if (plant_data_lock(10)) {
        plant_data_get_mutable()->dirty_flags |= DIRTY_ALARMS;
        plant_data_unlock();
    }
}

/* ---- Main router ---- */

void mqtt_handle_incoming(const char *topic, int topic_len,
                          const char *data, int data_len)
{
    char topic_buf[128];
    int len = (topic_len < (int)sizeof(topic_buf) - 1) ? topic_len : (int)sizeof(topic_buf) - 1;
    memcpy(topic_buf, topic, len);
    topic_buf[len] = '\0';

    cJSON *json = cJSON_ParseWithLength(data, data_len);
    if (!json) {
        ESP_LOGW(TAG, "JSON parse failed for %s", topic_buf);
        return;
    }

    if (strcmp(topic_buf, MQTT_TOPIC_STATE) == 0) {
        parse_state(json);
    } else if (strcmp(topic_buf, MQTT_TOPIC_IO) == 0) {
        parse_io(json);
    } else if (strncmp(topic_buf, MQTT_TOPIC_ANALOG_PREFIX,
                       strlen(MQTT_TOPIC_ANALOG_PREFIX)) == 0) {
        parse_analog(json, topic_buf + strlen(MQTT_TOPIC_ANALOG_PREFIX));
    } else if (strncmp(topic_buf, MQTT_TOPIC_FLOW_PREFIX,
                       strlen(MQTT_TOPIC_FLOW_PREFIX)) == 0) {
        parse_flow(json, topic_buf + strlen(MQTT_TOPIC_FLOW_PREFIX));
    } else if (strncmp(topic_buf, MQTT_TOPIC_COND_PREFIX,
                       strlen(MQTT_TOPIC_COND_PREFIX)) == 0) {
        parse_conductivity(json, topic_buf + strlen(MQTT_TOPIC_COND_PREFIX));
    } else if (strcmp(topic_buf, MQTT_TOPIC_TELEMETRY) == 0) {
        parse_telemetry(json);
    } else if (strcmp(topic_buf, MQTT_TOPIC_DOSER) == 0) {
        parse_doser(json);
    } else if (strcmp(topic_buf, MQTT_TOPIC_INTERLOCKS) == 0) {
        parse_interlocks(json);
    } else if (strcmp(topic_buf, MQTT_TOPIC_DIAGNOSTICS) == 0) {
        parse_diagnostics(json);
    } else if (strcmp(topic_buf, MQTT_TOPIC_ALARMS) == 0) {
        parse_alarm(json);
    }

    cJSON_Delete(json);
}
