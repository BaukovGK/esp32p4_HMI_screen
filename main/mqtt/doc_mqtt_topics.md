# Модуль mqtt_topics -- Определения MQTT-топиков

## Расположение файлов

- `main/mqtt/mqtt_topics.h` -- единственный файл (только макроопределения)

## Назначение

Заголовочный файл содержит все определения MQTT-топиков, используемых для связи между HMI-дисплеем (ESP32-P4) и основным контроллером RO-установки (ESP32-S3). Централизованное определение топиков исключает рассогласование строк в разных модулях.

## Подробное описание

### Иерархия топиков

```
ro_plant/
    status/                 -- статусные данные (контроллер -> HMI)
        state               -- состояние установки
        io                  -- цифровые входы/выходы
        analog/             -- аналоговые датчики
            P1              -- давление 1 (бар)
            P2              -- давление 2 (бар)
            P3              -- давление 3 (бар)
            P4              -- давление 4 (бар)
            T               -- температура (C)
        flow/               -- расходомеры
            Q1              -- расход 1 (м3/ч)
            Q2              -- расход 2 (м3/ч)
            Q3              -- расход 3 (м3/ч)
            Q4              -- расход 4 (м3/ч)
        conductivity/       -- кондуктометры
            s1              -- кондуктометр 1 (мкСм/см)
            s2              -- кондуктометр 2 (мкСм/см)
            s3              -- кондуктометр 3 (мкСм/см)
        telemetry           -- расчетная телеметрия
        doser               -- статус дозатора
        interlocks          -- блокировки
        diagnostics         -- диагностика контроллера
    alarms                  -- аварийные сообщения (контроллер -> HMI)
    command/                -- команды управления (HMI -> контроллер)
        mode                -- смена режима работы
        pump                -- управление насосами
        doser               -- управление дозатором
        heater              -- управление нагревателем
    settings/               -- настройки/уставки (HMI -> контроллер)
        pressure            -- уставки давления
        doser               -- настройки дозатора
        washing             -- настройки промывки
        timeouts            -- таймауты насосов
ro_hmi/
    availability            -- статус доступности HMI (online/offline)
```

## Макросы

### Топики подписки (Subscribe)

| Макрос | Значение | QoS | Описание |
|--------|----------|-----|----------|
| `MQTT_TOPIC_STATUS_ALL` | `"ro_plant/status/#"` | 0 | Wildcard-подписка на все статусные топики |
| `MQTT_TOPIC_ALARMS` | `"ro_plant/alarms"` | 1 | Аварийные сообщения |

### Статусные топики (для маршрутизации в парсере)

| Макрос | Значение | Тип маршрутизации |
|--------|----------|-------------------|
| `MQTT_TOPIC_STATE` | `"ro_plant/status/state"` | Точное совпадение (`strcmp`) |
| `MQTT_TOPIC_IO` | `"ro_plant/status/io"` | Точное совпадение (`strcmp`) |
| `MQTT_TOPIC_ANALOG_PREFIX` | `"ro_plant/status/analog/"` | Префикс (`strncmp`) + суффикс (P1..P4, T) |
| `MQTT_TOPIC_FLOW_PREFIX` | `"ro_plant/status/flow/"` | Префикс (`strncmp`) + суффикс (Q1..Q4) |
| `MQTT_TOPIC_COND_PREFIX` | `"ro_plant/status/conductivity/"` | Префикс (`strncmp`) + суффикс (s1..s3) |
| `MQTT_TOPIC_TELEMETRY` | `"ro_plant/status/telemetry"` | Точное совпадение (`strcmp`) |
| `MQTT_TOPIC_DOSER` | `"ro_plant/status/doser"` | Точное совпадение (`strcmp`) |
| `MQTT_TOPIC_INTERLOCKS` | `"ro_plant/status/interlocks"` | Точное совпадение (`strcmp`) |
| `MQTT_TOPIC_DIAGNOSTICS` | `"ro_plant/status/diagnostics"` | Точное совпадение (`strcmp`) |

### Топики команд (Publish от HMI)

| Макрос | Значение | Формат payload |
|--------|----------|----------------|
| `MQTT_TOPIC_CMD_MODE` | `"ro_plant/command/mode"` | Открытая строка (`"AUTO"`, `"IDLE"`, `"MANUAL"`, `"WASHING"`) |
| `MQTT_TOPIC_CMD_PUMP` | `"ro_plant/command/pump"` | JSON: `{"mask": N}` |
| `MQTT_TOPIC_CMD_DOSER` | `"ro_plant/command/doser"` | JSON: `{"enabled": bool}` |
| `MQTT_TOPIC_CMD_HEATER` | `"ro_plant/command/heater"` | JSON: `{"on": bool}` |

### Топики настроек (Publish от HMI)

| Макрос | Значение | Формат payload |
|--------|----------|----------------|
| `MQTT_TOPIC_SET_PRESSURE` | `"ro_plant/settings/pressure"` | JSON: `{"p1_max", "p3_max", "p4_max", "filter_dp_warn"}` (float, бар) |
| `MQTT_TOPIC_SET_DOSER` | `"ro_plant/settings/doser"` | JSON: `{"run_time_min", "cycle_time_min"}` (int, минуты) |
| `MQTT_TOPIC_SET_WASHING` | `"ro_plant/settings/washing"` | JSON: `{"target_temp_C", "max_temp_C", "t_overshoot_C", "hysteresis_C", "heat_timeout_min", "supply_time_min", "drain_time_min"}` |
| `MQTT_TOPIC_SET_TIMEOUTS` | `"ro_plant/settings/timeouts"` | JSON: `{"pump_confirm_ms", "pump_ramp_ms"}` (int, мс) |

## Направления потоков данных

```
ESP32-S3 (контроллер)                    ESP32-P4 (HMI дисплей)
         |                                        |
         | -- ro_plant/status/* ----------------> | (подписка, QoS 0)
         | -- ro_plant/alarms ------------------> | (подписка, QoS 1)
         |                                        |
         | <-- ro_plant/command/* ---------------- | (публикация, QoS 1)
         | <-- ro_plant/settings/* --------------- | (публикация, QoS 1)
         |                                        |
         |        ro_hmi/availability              | (LWT: "offline", online: "online")
```

## Зависимости

- Файл является чистым заголовочным файлом с `#define` макросами
- Не зависит от других модулей
- Используется в:
  - `mqtt_app.c` -- подписка и публикация
  - `mqtt_parser.c` -- маршрутизация входящих сообщений

## Связь с другими модулями

- **mqtt_app.c**: Использует топики подписки (`MQTT_TOPIC_STATUS_ALL`, `MQTT_TOPIC_ALARMS`) и топики публикации (`MQTT_TOPIC_CMD_*`, `MQTT_TOPIC_SET_*`)
- **mqtt_parser.c**: Использует статусные топики и префиксы для маршрутизации входящих сообщений к соответствующим парсерам

## Примечания

- Топик `ro_hmi/availability` определен как строковый литерал непосредственно в `mqtt_app.c`, а не как макрос в данном файле
- Wildcard-подписка `ro_plant/status/#` захватывает все параметрические топики (analog/P1, flow/Q1 и т.д.), что упрощает код подписки до одной строки
- Префиксные топики (`*_PREFIX`) заканчиваются на `/` и используются для извлечения имени датчика из суффикса при маршрутизации
