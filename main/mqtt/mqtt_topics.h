/**
 * @file mqtt_topics.h
 * @brief Определения MQTT-топиков для связи с контроллером RO-установки.
 *
 * Все топики организованы в иерархию:
 * - ro_plant/status/#      -- статусная информация (HMI подписывается)
 * - ro_plant/alarms        -- аварийные сообщения (HMI подписывается)
 * - ro_plant/command/...   -- команды управления (HMI публикует)
 * - ro_plant/settings/...  -- настройки/уставки (HMI публикует)
 * - ro_hmi/availability    -- статус доступности HMI (online/offline)
 *
 * ВАЖНО: Все макросы топиков — null-terminated string literals. При добавлении
 * нового топика убедитесь что его длина < MQTT_TOPIC_BUF_SIZE; либо добавьте
 * _Static_assert ниже в конце файла.
 */
#pragma once

/* Топики подписки (subscribe) */

/** Подписка на все статусные топики (wildcard #) */
#define MQTT_TOPIC_STATUS_ALL       "ro_plant/status/#"

/** Подписка на аварийные сообщения */
#define MQTT_TOPIC_ALARMS           "ro_plant/alarms"

/** Availability контроллера (LWT). Контроллер публикует "online"/"offline". */
#define MQTT_TOPIC_AVAILABILITY     "ro_plant/availability"

/* Статусные топики (для маршрутизации входящих сообщений) */

/** Состояние установки: state, auto_sub, wash_sub, fault_flags */
#define MQTT_TOPIC_STATE            "ro_plant/status/state"

/** Цифровые входы/выходы: di, do */
#define MQTT_TOPIC_IO               "ro_plant/status/io"

/** Префикс аналоговых датчиков (+ P1, P2, P3, P4, T) */
#define MQTT_TOPIC_ANALOG_PREFIX    "ro_plant/status/analog/"

/** Префикс расходомеров (+ Q1, Q2, Q3, Q4) */
#define MQTT_TOPIC_FLOW_PREFIX      "ro_plant/status/flow/"

/** Префикс кондуктометров (+ s1, s2, s3, s4) — s4 = концентрат (с 2026-05-09) */
#define MQTT_TOPIC_COND_PREFIX      "ro_plant/status/conductivity/"

/**
 * Префикс счётчиков электроэнергии KWS-306L (+ lp, hp).
 * lp = низкого давления (НД-насос), hp = высокого давления (ВД-насос).
 */
#define MQTT_TOPIC_POWER_PREFIX     "ro_plant/status/power/"

/** Расчетная телеметрия: filter_dp, stage1_feed, recovery, selectivity */
#define MQTT_TOPIC_TELEMETRY        "ro_plant/status/telemetry"

/** Статус дозатора: state, enabled */
#define MQTT_TOPIC_DOSER            "ro_plant/status/doser"

/** Блокировки: flags, estop, filter_warn */
#define MQTT_TOPIC_INTERLOCKS       "ro_plant/status/interlocks"

/** Диагностика контроллера: heap, uptime, stack, modbus */
#define MQTT_TOPIC_DIAGNOSTICS      "ro_plant/status/diagnostics"

/* Топики команд управления (publish от HMI) */

/** Команда смены режима: "AUTO", "IDLE", "MANUAL", "WASHING" */
#define MQTT_TOPIC_CMD_MODE         "ro_plant/command/mode"

/** Команда управления насосами: {"mask": N} */
#define MQTT_TOPIC_CMD_PUMP         "ro_plant/command/pump"

/** Команда управления дозатором: {"enabled": true/false} */
#define MQTT_TOPIC_CMD_DOSER        "ro_plant/command/doser"

/** Команда управления нагревателем: {"on": true/false} */
#define MQTT_TOPIC_CMD_HEATER       "ro_plant/command/heater"

/* Топики настроек/уставок (publish от HMI) */

/** Настройки давления: p1_max, p3_max, p4_max, filter_dp_warn */
#define MQTT_TOPIC_SET_PRESSURE     "ro_plant/settings/pressure"

/** Настройки дозатора: run_time_min, cycle_time_min */
#define MQTT_TOPIC_SET_DOSER        "ro_plant/settings/doser"

/** Настройки промывки: температуры, таймауты */
#define MQTT_TOPIC_SET_WASHING      "ro_plant/settings/washing"

/** Настройки таймаутов насосов: pump_confirm_ms, pump_ramp_ms */
#define MQTT_TOPIC_SET_TIMEOUTS     "ro_plant/settings/timeouts"

/* Топик доступности HMI (Last Will + online) */

/** Статус доступности HMI-дисплея: "online" / "offline" (retained, LWT) */
#define MQTT_TOPIC_HMI_AVAILABILITY "ro_hmi/availability"

/* ---- Размер буфера для копирования топика в парсере ---- */

/** Размер буфера для копирования топика в парсере.
 *  Гарантировано вмещает все определённые топики этого header'а. */
#define MQTT_TOPIC_BUF_SIZE 128

/* Compile-time проверка что самый длинный топик помещается в буфер.
 * Самые длинные:
 *  ro_plant/status/conductivity/s4    = 32 байта
 *  ro_plant/status/diagnostics        = 28 байт
 *  ro_plant/status/power/lp           = 25 байт
 * Запас MQTT_TOPIC_BUF_SIZE - max_topic_len ~= 96 байт хватает на любые
 * будущие расширения топик-структуры. */
_Static_assert(sizeof("ro_plant/status/conductivity/s4") <= MQTT_TOPIC_BUF_SIZE,
               "MQTT_TOPIC_BUF_SIZE слишком мал для conductivity/s4");
_Static_assert(sizeof("ro_plant/status/diagnostics") <= MQTT_TOPIC_BUF_SIZE,
               "MQTT_TOPIC_BUF_SIZE слишком мал для diagnostics");
_Static_assert(sizeof("ro_plant/status/power/lp") <= MQTT_TOPIC_BUF_SIZE,
               "MQTT_TOPIC_BUF_SIZE слишком мал для power/lp");
