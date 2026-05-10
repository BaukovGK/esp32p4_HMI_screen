/**
 * @file mqtt_parser.c
 * @brief Парсер входящих MQTT-сообщений для HMI-дисплея RO-установки.
 *
 * Маршрутизирует входящие MQTT-сообщения по топикам и разбирает
 * JSON-payload с помощью библиотеки cJSON. Распарсенные данные
 * сохраняются в потокобезопасное хранилище plant_data.
 *
 * Обрабатываемые топики:
 * - ro_plant/status/state          -- состояние и подсостояния установки
 * - ro_plant/status/io             -- цифровые входы/выходы
 * - ro_plant/status/analog/{P1..P4,T} -- аналоговые датчики
 * - ro_plant/status/flow/{Q1..Q4}  -- расходомеры
 * - ro_plant/status/conductivity/{s1..s4} -- кондуктометры (s4=концентрат)
 * - ro_plant/status/power/{lp,hp}  -- счётчики KWS-306L (НД и ВД-насос)
 * - ro_plant/status/telemetry      -- расчетная телеметрия
 * - ro_plant/status/doser          -- статус дозатора
 * - ro_plant/status/interlocks     -- блокировки
 * - ro_plant/status/diagnostics    -- диагностика контроллера
 * - ro_plant/alarms                -- аварийные сообщения
 * - ro_plant/availability          -- "online"/"offline" контроллера (НЕ JSON)
 */
#ifndef LVGL_LIVE_PREVIEW
#include "mqtt_parser.h"
#include "mqtt_topics.h"
#include "plant_data.h"
#include "alarm_ring.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "mqtt_parse";

/* ---- Вспомогательные функции извлечения данных из JSON ---- */

/**
 * Извлечение float-значения из JSON-объекта по ключу.
 *
 * @param obj  JSON-объект
 * @param key  Имя ключа
 * @param def  Значение по умолчанию (если ключ отсутствует)
 * @return Значение ключа; NAN если значение null; def если ключ не найден
 */
static float json_get_float(const cJSON *obj, const char *key, float def)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) return def;
    if (cJSON_IsNull(item)) return NAN;  // null в JSON -> NAN (датчик недоступен)
    if (cJSON_IsNumber(item)) return (float)item->valuedouble;
    return def;
}

/**
 * Извлечение bool-значения из JSON-объекта по ключу.
 *
 * @param obj  JSON-объект
 * @param key  Имя ключа
 * @param def  Значение по умолчанию (если ключ отсутствует)
 * @return true/false из JSON, или def если ключ не найден
 */
static bool json_get_bool(const cJSON *obj, const char *key, bool def)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) return def;
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
    return def;
}

/**
 * Извлечение int-значения из JSON-объекта по ключу.
 *
 * @param obj  JSON-объект
 * @param key  Имя ключа
 * @param def  Значение по умолчанию (если ключ отсутствует или не число)
 * @return Целочисленное значение из JSON, или def если ключ не найден
 */
static int json_get_int(const cJSON *obj, const char *key, int def)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    return item->valueint;
}

/** Получение int64 значения из JSON (через valuedouble для большого диапазона). */
static int64_t json_get_int64(const cJSON *obj, const char *key, int64_t def)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    return (int64_t)item->valuedouble;
}

/**
 * Извлечение строкового значения из JSON-объекта по ключу.
 *
 * @param obj  JSON-объект
 * @param key  Имя ключа
 * @param def  Значение по умолчанию (если ключ отсутствует или не строка)
 * @return Указатель на строку внутри cJSON-объекта (не копия!), или def
 */
static const char *json_get_string(const cJSON *obj, const char *key, const char *def)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsString(item)) return def;
    return item->valuestring;
}

/* ---- Парсер состояния установки ---- */

/**
 * Преобразование строки состояния в перечисление plant_state_t.
 *
 * Поддерживаемые значения: "IDLE", "AUTO", "WASHING", "MANUAL", "FAULT".
 */
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

/**
 * Преобразование строки подсостояния AUTO в перечисление auto_sub_state_t.
 *
 * Подсостояния автоматического режима:
 * STARTING_PUMP1 -> RAMP -> STARTING_PUMP2 -> FILLING_INTERM ->
 * STARTING_PUMP3 -> RUNNING -> STOPPING
 */
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

/**
 * Преобразование строки подсостояния WASHING в перечисление wash_sub_state_t.
 *
 * Подсостояния режима промывки:
 * WAIT_HEAT -> HEATING -> WAIT_SUPPLY -> SUPPLY -> WAIT_DRAIN -> DRAIN -> DONE
 */
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

/**
 * Парсинг сообщения состояния установки (ro_plant/status/state).
 *
 * JSON: {"state":"AUTO", "auto_sub":"RUNNING", "wash_sub":null, "fault_flags":0}
 * Обновляет состояние, подсостояния AUTO/WASHING и флаги неисправностей.
 *
 * ВАЖНО: если обязательное поле "state" отсутствует или не валидно — сообщение
 * игнорируется целиком (без вызова setter), чтобы битый JSON не затирал
 * корректное состояние установки. PLANT_STATE_UNKNOWN — это валидный сигнал
 * "контроллер не знает своё состояние", а не "забудь всё".
 */
static void parse_state(const cJSON *json)
{
    /* Сначала прочитать ВСЕ поля, потом валидировать, потом setter. */
    const char *state_str = json_get_string(json, "state", NULL);
    if (!state_str) {
        ESP_LOGW(TAG, "parse_state: missing 'state' field, ignoring");
        return;
    }
    plant_state_t st = parse_state_enum(state_str);
    /* Если строка есть, но не парсится в один из 5 валидных state — тоже дроп. */
    if (st == PLANT_STATE_UNKNOWN && strcmp(state_str, "UNKNOWN") != 0) {
        ESP_LOGW(TAG, "parse_state: invalid state='%s', ignoring", state_str);
        return;
    }
    auto_sub_state_t asub = parse_auto_sub(json_get_string(json, "auto_sub", NULL));
    wash_sub_state_t wsub = parse_wash_sub(json_get_string(json, "wash_sub", NULL));
    uint32_t ff = (uint32_t)json_get_int(json, "fault_flags", 0);
    plant_data_set_state(st, asub, wsub, ff);
}

/* ---- Парсер цифровых входов/выходов ---- */

/**
 * Парсинг сообщения I/O (ro_plant/status/io).
 *
 * JSON: {"di": 0xFF, "do": 0x0A}
 * di -- 8-битная маска цифровых входов, do -- 8-битная маска цифровых выходов
 */
static void parse_io(const cJSON *json)
{
    uint8_t di = (uint8_t)json_get_int(json, "di", 0);
    uint8_t do_bits = (uint8_t)json_get_int(json, "do", 0);
    plant_data_set_io(di, do_bits);
}

/* ---- Парсер аналоговых датчиков ---- */

/**
 * Парсинг сообщения аналогового датчика (ro_plant/status/analog/{name}).
 *
 * JSON: {"value": 3.5, "fault": false}
 * Суффикс топика определяет тип датчика:
 * - P1..P4 -- датчики давления (бар)
 * - T      -- датчик температуры (C)
 *
 * @param json JSON-объект сообщения
 * @param name Суффикс топика (например, "P1", "T")
 */
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

/* ---- Парсер расходомеров ---- */

/**
 * Парсинг сообщения расходомера (ro_plant/status/flow/{name}).
 *
 * JSON: {"flow": 1.2, "volume": 345.6, "ok": true}
 * - flow   -- мгновенный расход (м3/ч)
 * - volume -- накопленный объем (м3)
 * Суффикс топика: Q1..Q4 -> индекс 0..3
 *
 * @param json JSON-объект сообщения
 * @param name Суффикс топика (например, "Q1")
 */
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

/* ---- Парсер кондуктометров ---- */

/**
 * Парсинг сообщения кондуктометра (ro_plant/status/conductivity/{name}).
 *
 * JSON: {"conductivity": 450.0, "temperature": 25.3, "ok": true}
 * - conductivity -- удельная электропроводность (мкСм/см)
 * - temperature  -- температура раствора (C)
 * Суффикс топика: s1..s4 -> индекс 0..3 (s4 — концентрат, добавлен 2026-05-09).
 *
 * @param json JSON-объект сообщения
 * @param name Суффикс топика (например, "s1")
 */
static void parse_conductivity(const cJSON *json, const char *name)
{
    float c = json_get_float(json, "conductivity", NAN);
    float t = json_get_float(json, "temperature", NAN);
    bool ok = json_get_bool(json, "ok", false);

    int idx = -1;
    if (strcmp(name, "s1") == 0)      idx = 0;
    else if (strcmp(name, "s2") == 0) idx = 1;
    else if (strcmp(name, "s3") == 0) idx = 2;
    else if (strcmp(name, "s4") == 0) idx = 3;

    if (idx >= 0) plant_data_set_conductivity(idx, c, t, ok);
}

/* ---- Парсер счётчика электроэнергии KWS-306L ---- */

/**
 * Парсинг сообщения KWS-306L (ro_plant/status/power/{name}).
 *
 * JSON: {"voltage":230.5, "current":5.2, "power":1205.3,
 *        "energy":12.45, "temperature":45.2, "online":true}
 * Любое из полей V/A/W/kWh/T может быть null, что трактуется как NAN
 * ("датчик не отвечает / нет данных"). Поле online — обязательный bool.
 *
 * @param json JSON-объект сообщения
 * @param name Суффикс топика: "lp" (НД-насос) или "hp" (ВД-насос)
 */
static void parse_power(const cJSON *json, const char *name)
{
    int idx = -1;
    if (strcmp(name, "lp") == 0)      idx = 0;
    else if (strcmp(name, "hp") == 0) idx = 1;
    if (idx < 0) return;

    power_meter_data_t pm = {
        .voltage     = json_get_float(json, "voltage",     NAN),
        .current     = json_get_float(json, "current",     NAN),
        .power       = json_get_float(json, "power",       NAN),
        .energy      = json_get_float(json, "energy",      NAN),
        .temperature = json_get_float(json, "temperature", NAN),
        .online      = json_get_bool (json, "online",      false),
    };
    plant_data_set_power_meter(idx, &pm);
}

/* ---- Парсер телеметрии ---- */

/**
 * Парсинг сообщения расчетной телеметрии (ro_plant/status/telemetry).
 *
 * JSON: {"filter_dp":0.5, "stage1_feed":2.1, "recovery2":75.0,
 *        "recovery_sys":85.0, "sel1":98.5, "sel2":99.1}
 * - filter_dp    -- перепад давления на фильтре (бар, P1-P2)
 * - stage1_feed  -- подача на 1 ступень (м3/ч, Q1+Q2)
 * - recovery2    -- выход 2 ступени (%)
 * - recovery_sys -- системный выход (%)
 * - sel1, sel2   -- селективность ступеней (%)
 */
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

/* ---- Парсер статуса дозатора ---- */

/**
 * Парсинг сообщения дозатора (ro_plant/status/doser).
 *
 * JSON: {"state":"RUNNING", "enabled":true}
 * - state   -- состояние: "OFF", "RUNNING", "PAUSE"
 * - enabled -- дозирование разрешено оператором
 */
static void parse_doser(const cJSON *json)
{
    const char *st = json_get_string(json, "state", "OFF");
    doser_state_t ds = DOSER_STATE_OFF;
    if (strcmp(st, "RUNNING") == 0) ds = DOSER_STATE_RUNNING;
    else if (strcmp(st, "PAUSE") == 0) ds = DOSER_STATE_PAUSE;

    bool en = json_get_bool(json, "enabled", false);
    plant_data_set_doser(ds, en);
}

/* ---- Парсер блокировок ---- */

/**
 * Парсинг сообщения блокировок (ro_plant/status/interlocks).
 *
 * JSON: {"flags": 0, "estop": false, "filter_warn": false}
 * - flags       -- битовая маска активных блокировок
 * - estop       -- аварийная остановка (кнопка E-STOP)
 * - filter_warn -- предупреждение о загрязнении фильтра
 */
static void parse_interlocks(const cJSON *json)
{
    uint32_t flags = (uint32_t)json_get_int(json, "flags", 0);
    bool estop = json_get_bool(json, "estop", false);
    bool fw = json_get_bool(json, "filter_warn", false);
    plant_data_set_interlocks(flags, estop, fw);
}

/* ---- Парсер диагностики контроллера ---- */

/**
 * Парсинг сообщения диагностики (ro_plant/status/diagnostics).
 *
 * JSON содержит: heap_free, heap_min, uptime_s, wdt_stale,
 * вложенный объект stack (остатки стеков задач),
 * вложенный объект modbus (ошибки и статус устройств на шине).
 */
static void parse_diagnostics(const cJSON *json)
{
    diagnostics_t diag = {0};
    diag.heap_free = (uint32_t)json_get_int(json, "heap_free", 0);
    diag.heap_min  = (uint32_t)json_get_int(json, "heap_min", 0);
    diag.uptime_s  = json_get_int64(json, "uptime_s", 0);
    diag.wdt_stale = (uint32_t)json_get_int(json, "wdt_stale", 0);

    // Парсинг вложенного объекта остатков стеков задач.
    // Ключи стека динамические (имена FreeRTOS-задач) — мэппим по фактическим именам.
    cJSON *stack = cJSON_GetObjectItemCaseSensitive(json, "stack");
    if (stack && cJSON_IsObject(stack)) {
        diag.stack_modbus   = (uint32_t)json_get_int(stack, "modbus", 0);
        diag.stack_io       = (uint32_t)json_get_int(stack, "io", 0);
        diag.stack_process  = (uint32_t)json_get_int(stack, "process", 0);
        diag.stack_watchdog = (uint32_t)json_get_int(stack, "watchdog", 0);
        diag.stack_mqtt     = (uint32_t)json_get_int(stack, "mqtt", 0);
    }

    /*
     * Парсинг массива Modbus-устройств. Контроллер с 2026-05-09 публикует
     * "modbus" как массив объектов: [{"addr":N,"errors":N,"online":true}, ...]
     * (раньше были два параллельных массива errors[] и online[]).
     * Берём первые 4 устройства; сохраняем индексы в plant_data в том порядке,
     * в каком пришли (UI пока не показывает adr — только сам факт ошибок/online).
     */
    cJSON *modbus = cJSON_GetObjectItemCaseSensitive(json, "modbus");
    if (modbus && cJSON_IsArray(modbus)) {
        int total = cJSON_GetArraySize(modbus);
        int n = total < 4 ? total : 4;
        for (int i = 0; i < n; i++) {
            cJSON *dev = cJSON_GetArrayItem(modbus, i);
            if (!dev || !cJSON_IsObject(dev)) continue;
            diag.modbus_errors[i] = (uint32_t)json_get_int(dev, "errors", 0);
            diag.modbus_online[i] = json_get_bool(dev, "online", false);
        }
        // Оставшиеся слоты (если устройств меньше 4) сбросить в "оффлайн без ошибок"
        for (int i = n; i < 4; i++) {
            diag.modbus_errors[i] = 0;
            diag.modbus_online[i] = false;
        }
    }

    plant_data_set_diagnostics(&diag);
}

/* ---- Парсер аварийных сообщений ---- */

/**
 * Преобразование строки категории аварии в перечисление alarm_category_t.
 *
 * Категории (по возрастанию критичности):
 * INFO -> WARNING -> ALARM -> CRITICAL
 */
static alarm_category_t parse_alarm_cat(const char *s)
{
    if (!s) return ALARM_CAT_INFO;
    if (strcmp(s, "CRITICAL") == 0) return ALARM_CAT_CRITICAL;
    if (strcmp(s, "ALARM") == 0)    return ALARM_CAT_ALARM;
    if (strcmp(s, "WARNING") == 0)  return ALARM_CAT_WARNING;
    return ALARM_CAT_INFO;
}

/**
 * Парсинг аварийного сообщения (ro_plant/alarms).
 *
 * JSON: {"id":1, "ts":1700000000, "cat":"ALARM", "code":"HIGH_P1",
 *        "value":12.5, "active":true}
 *
 * Авария добавляется в кольцевой буфер alarm_ring и
 * выставляется флаг DIRTY_ALARMS для обновления UI.
 */
static void parse_alarm(const cJSON *json)
{
    alarm_entry_t entry = {0};
    entry.id = (uint32_t)json_get_int(json, "id", 0);
    entry.ts = json_get_int64(json, "ts", 0);
    entry.cat = parse_alarm_cat(json_get_string(json, "cat", "INFO"));
    entry.value = json_get_float(json, "value", 0);
    entry.active = json_get_bool(json, "active", true);

    const char *code = json_get_string(json, "code", "UNKNOWN");
    strncpy(entry.code, code, sizeof(entry.code) - 1);

    /*
     * ВАЖНО: alarm_ring lock не вкладывается в plant_data lock (порядок мьютексов).
     * Это два независимых мьютекса; вложенный захват любых двух из них —
     * потенциальный deadlock, если другая задача в этот момент берёт их в
     * обратном порядке. Поэтому сначала push в alarm_ring (со своим мьютексом),
     * потом отдельно — установка dirty-флага в plant_data.
     */
    alarm_ring_push(&entry);

    if (plant_data_lock(10)) {
        plant_data_get_mutable()->dirty_flags |= DIRTY_ALARMS;
        plant_data_unlock();
    } else {
        ESP_LOGW(TAG, "parse_alarm: plant_data lock timeout, UI may miss dirty bit");
    }
}

/* ---- Главный маршрутизатор входящих MQTT-сообщений ---- */

/**
 * Маршрутизация и обработка входящего MQTT-сообщения.
 *
 * Вызывается из обработчика событий MQTT при получении MQTT_EVENT_DATA.
 * Копирует топик в локальный буфер, разбирает JSON-payload через cJSON,
 * определяет тип сообщения по топику и вызывает соответствующий парсер.
 *
 * Маршрутизация:
 * - Точное совпадение: state, io, telemetry, doser, interlocks, diagnostics, alarms
 * - По префиксу: analog/{name}, flow/{name}, conductivity/{name}
 *
 * @param topic     Указатель на строку топика (может быть без терминатора)
 * @param topic_len Длина строки топика
 * @param data      Указатель на JSON-payload (может быть без терминатора)
 * @param data_len  Длина JSON-payload
 */
void mqtt_handle_incoming(const char *topic, int topic_len,
                          const char *data, int data_len)
{
    // Копирование топика в локальный буфер с нуль-терминатором
    char topic_buf[MQTT_TOPIC_BUF_SIZE];
    int len = (topic_len < (int)sizeof(topic_buf) - 1) ? topic_len : (int)sizeof(topic_buf) - 1;
    memcpy(topic_buf, topic, len);
    topic_buf[len] = '\0';

    /*
     * Availability контроллера — это plain-text ("online" / "offline"),
     * не JSON. Обрабатываем до cJSON_ParseWithLength, чтобы не получить
     * ложную ошибку парсинга. Записываем флаг controller_online в plant_data,
     * чтобы UI мог явно показывать статус (а не только по last_msg_time_us).
     */
    if (strcmp(topic_buf, MQTT_TOPIC_AVAILABILITY) == 0) {
        ESP_LOGI(TAG, "controller availability=%.*s", data_len, data);
        bool online = (data_len >= 6 && strncmp(data, "online", 6) == 0);
        plant_data_set_controller_online(online);
        return;
    }

    // Парсинг JSON-payload (cJSON требует данные с известной длиной)
    cJSON *json = cJSON_ParseWithLength(data, data_len);
    if (!json) {
        ESP_LOGW(TAG, "JSON parse failed for %s", topic_buf);
        return;
    }

    // Маршрутизация по топику
    if (strcmp(topic_buf, MQTT_TOPIC_STATE) == 0) {
        parse_state(json);                                          // Состояние установки
    } else if (strcmp(topic_buf, MQTT_TOPIC_IO) == 0) {
        parse_io(json);                                             // Цифровые входы/выходы
    } else if (strncmp(topic_buf, MQTT_TOPIC_ANALOG_PREFIX,
                       strlen(MQTT_TOPIC_ANALOG_PREFIX)) == 0) {
        parse_analog(json, topic_buf + strlen(MQTT_TOPIC_ANALOG_PREFIX)); // Аналоговые датчики
    } else if (strncmp(topic_buf, MQTT_TOPIC_FLOW_PREFIX,
                       strlen(MQTT_TOPIC_FLOW_PREFIX)) == 0) {
        parse_flow(json, topic_buf + strlen(MQTT_TOPIC_FLOW_PREFIX));     // Расходомеры
    } else if (strncmp(topic_buf, MQTT_TOPIC_COND_PREFIX,
                       strlen(MQTT_TOPIC_COND_PREFIX)) == 0) {
        parse_conductivity(json, topic_buf + strlen(MQTT_TOPIC_COND_PREFIX)); // Кондуктометры
    } else if (strncmp(topic_buf, MQTT_TOPIC_POWER_PREFIX,
                       strlen(MQTT_TOPIC_POWER_PREFIX)) == 0) {
        parse_power(json, topic_buf + strlen(MQTT_TOPIC_POWER_PREFIX));   // KWS-306L lp/hp
    } else if (strcmp(topic_buf, MQTT_TOPIC_TELEMETRY) == 0) {
        parse_telemetry(json);                                      // Телеметрия
    } else if (strcmp(topic_buf, MQTT_TOPIC_DOSER) == 0) {
        parse_doser(json);                                          // Дозатор
    } else if (strcmp(topic_buf, MQTT_TOPIC_INTERLOCKS) == 0) {
        parse_interlocks(json);                                     // Блокировки
    } else if (strcmp(topic_buf, MQTT_TOPIC_DIAGNOSTICS) == 0) {
        parse_diagnostics(json);                                    // Диагностика
    } else if (strcmp(topic_buf, MQTT_TOPIC_ALARMS) == 0) {
        parse_alarm(json);                                          // Аварии
    }

    // Освобождение памяти JSON-объекта
    cJSON_Delete(json);
}
#endif /* !LVGL_LIVE_PREVIEW */
