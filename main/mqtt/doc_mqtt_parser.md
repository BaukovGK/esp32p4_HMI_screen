# Модуль mqtt_parser -- Парсер входящих MQTT-сообщений

## Расположение файлов

- `main/mqtt/mqtt_parser.c` -- реализация
- `main/mqtt/mqtt_parser.h` -- заголовочный файл (публичный API)

## Назначение

Модуль отвечает за маршрутизацию входящих MQTT-сообщений по топикам и разбор JSON-payload. Извлеченные данные сохраняются в потокобезопасное хранилище `plant_data` и кольцевой буфер аварий `alarm_ring`.

## Подробное описание

### Архитектура парсера

Парсер реализован как набор специализированных функций-обработчиков, вызываемых из главного маршрутизатора `mqtt_handle_incoming()`:

```
mqtt_handle_incoming()
    |
    +-- cJSON_ParseWithLength()     -- разбор JSON
    |
    +-- Маршрутизация по топику:
        +-- parse_state()           -- состояние установки
        +-- parse_io()              -- цифровые входы/выходы
        +-- parse_analog()          -- аналоговые датчики (P1-P4, T)
        +-- parse_flow()            -- расходомеры (Q1-Q4)
        +-- parse_conductivity()    -- кондуктометры (s1-s3)
        +-- parse_telemetry()       -- расчетная телеметрия
        +-- parse_doser()           -- статус дозатора
        +-- parse_interlocks()      -- блокировки
        +-- parse_diagnostics()     -- диагностика контроллера
        +-- parse_alarm()           -- аварийные сообщения
    |
    +-- cJSON_Delete()              -- освобождение памяти
```

### Подход к парсингу JSON

1. **Библиотека**: Используется cJSON (входит в ESP-IDF)
2. **Безопасный парсинг**: `cJSON_ParseWithLength()` -- работает с данными без нуль-терминатора
3. **Регистрозависимый поиск ключей**: `cJSON_GetObjectItemCaseSensitive()` -- быстрее чем регистронезависимый
4. **Значения по умолчанию**: Каждый хелпер возвращает значение по умолчанию если ключ отсутствует
5. **Обработка null**: JSON-значение `null` для float преобразуется в `NAN` (датчик недоступен)
6. **Вложенные объекты**: Диагностика содержит вложенные объекты `stack` и `modbus`

### Маршрутизация топиков

- **Точное совпадение**: `strcmp()` для топиков без параметров (state, io, telemetry и т.д.)
- **Совпадение по префиксу**: `strncmp()` для параметрических топиков (analog/, flow/, conductivity/), суффикс извлекается как имя датчика

## Функции

### Публичные

#### `void mqtt_handle_incoming(const char *topic, int topic_len, const char *data, int data_len)`

Главный маршрутизатор входящих MQTT-сообщений. Копирует топик в локальный буфер (128 байт), парсит JSON, определяет обработчик по топику.

- **Параметры**:
  - `topic` -- указатель на строку топика (без нуль-терминатора)
  - `topic_len` -- длина строки топика
  - `data` -- указатель на JSON-payload (без нуль-терминатора)
  - `data_len` -- длина JSON-payload

### Внутренние (static)

#### Вспомогательные функции JSON

| Функция | Описание | Возврат при null в JSON |
|---------|----------|------------------------|
| `json_get_float(obj, key, def)` | Извлечение float | `NAN` |
| `json_get_bool(obj, key, def)` | Извлечение bool | `def` |
| `json_get_int(obj, key, def)` | Извлечение int | `def` |
| `json_get_string(obj, key, def)` | Извлечение строки (указатель, не копия) | `def` |

#### Парсеры перечислений

| Функция | Вход (строка) | Выход (enum) |
|---------|---------------|--------------|
| `parse_state_enum(s)` | `"IDLE"`, `"AUTO"`, `"WASHING"`, `"MANUAL"`, `"FAULT"` | `plant_state_t` |
| `parse_auto_sub(s)` | `"STARTING_PUMP1"`, `"RAMP"`, `"STARTING_PUMP2"`, `"FILLING_INTERM"`, `"STARTING_PUMP3"`, `"RUNNING"`, `"STOPPING"` | `auto_sub_state_t` |
| `parse_wash_sub(s)` | `"WAIT_HEAT"`, `"HEATING"`, `"WAIT_SUPPLY"`, `"SUPPLY"`, `"WAIT_DRAIN"`, `"DRAIN"`, `"DONE"` | `wash_sub_state_t` |
| `parse_alarm_cat(s)` | `"INFO"`, `"WARNING"`, `"ALARM"`, `"CRITICAL"` | `alarm_category_t` |

#### Парсеры данных

| Функция | Топик | Обновляемые данные |
|---------|-------|--------------------|
| `parse_state(json)` | `ro_plant/status/state` | `plant_data.state`, `auto_sub`, `wash_sub`, `fault_flags` |
| `parse_io(json)` | `ro_plant/status/io` | `plant_data.di`, `do_bits` |
| `parse_analog(json, name)` | `ro_plant/status/analog/{name}` | `plant_data.pressure[0..3]`, `temperature` |
| `parse_flow(json, name)` | `ro_plant/status/flow/{name}` | `plant_data.flow[0..3]` |
| `parse_conductivity(json, name)` | `ro_plant/status/conductivity/{name}` | `plant_data.conductivity[0..2]` |
| `parse_telemetry(json)` | `ro_plant/status/telemetry` | `plant_data.telemetry` |
| `parse_doser(json)` | `ro_plant/status/doser` | `plant_data.doser` |
| `parse_interlocks(json)` | `ro_plant/status/interlocks` | `plant_data.interlocks` |
| `parse_diagnostics(json)` | `ro_plant/status/diagnostics` | `plant_data.diagnostics` |
| `parse_alarm(json)` | `ro_plant/alarms` | `alarm_ring` + `DIRTY_ALARMS` |

## Форматы JSON-сообщений

### ro_plant/status/state

```json
{
  "state": "AUTO",
  "auto_sub": "RUNNING",
  "wash_sub": null,
  "fault_flags": 0
}
```

Поля:
- `state` -- основное состояние: `"IDLE"`, `"AUTO"`, `"WASHING"`, `"MANUAL"`, `"FAULT"`
- `auto_sub` -- подсостояние AUTO: `"STARTING_PUMP1"`, `"RAMP"`, `"STARTING_PUMP2"`, `"FILLING_INTERM"`, `"STARTING_PUMP3"`, `"RUNNING"`, `"STOPPING"`
- `wash_sub` -- подсостояние WASHING: `"WAIT_HEAT"`, `"HEATING"`, `"WAIT_SUPPLY"`, `"SUPPLY"`, `"WAIT_DRAIN"`, `"DRAIN"`, `"DONE"`
- `fault_flags` -- битовая маска неисправностей (uint32)

### ro_plant/status/io

```json
{
  "di": 255,
  "do": 10
}
```

Поля:
- `di` -- 8-битная маска цифровых входов
- `do` -- 8-битная маска цифровых выходов

### ro_plant/status/analog/{P1|P2|P3|P4|T}

```json
{
  "value": 3.5,
  "fault": false
}
```

Поля:
- `value` -- значение в инженерных единицах (бар для P1-P4, C для T); `null` означает неисправность датчика (преобразуется в `NAN`)
- `fault` -- признак обрыва линии датчика

### ro_plant/status/flow/{Q1|Q2|Q3|Q4}

```json
{
  "flow": 1.2,
  "volume": 345.6,
  "ok": true
}
```

Поля:
- `flow` -- мгновенный расход (м3/ч)
- `volume` -- накопленный объем (м3)
- `ok` -- канал исправен

### ro_plant/status/conductivity/{s1|s2|s3}

```json
{
  "conductivity": 450.0,
  "temperature": 25.3,
  "ok": true
}
```

Поля:
- `conductivity` -- удельная электропроводность (мкСм/см)
- `temperature` -- температура раствора (C)
- `ok` -- канал исправен

### ro_plant/status/telemetry

```json
{
  "filter_dp": 0.5,
  "stage1_feed": 2.1,
  "recovery2": 75.0,
  "recovery_sys": 85.0,
  "sel1": 98.5,
  "sel2": 99.1
}
```

Поля:
- `filter_dp` -- перепад давления на фильтре (бар, P1-P2)
- `stage1_feed` -- подача на 1 ступень (м3/ч, Q1+Q2)
- `recovery2` -- выход 2 ступени (%, Q3/(Q3+Q4)*100)
- `recovery_sys` -- системный выход (%, Q3/Q1*100)
- `sel1` -- селективность 1 ступени (%)
- `sel2` -- селективность 2 ступени (%)

### ro_plant/status/doser

```json
{
  "state": "RUNNING",
  "enabled": true
}
```

Поля:
- `state` -- состояние дозатора: `"OFF"`, `"RUNNING"`, `"PAUSE"`
- `enabled` -- дозирование разрешено оператором

### ro_plant/status/interlocks

```json
{
  "flags": 0,
  "estop": false,
  "filter_warn": false
}
```

Поля:
- `flags` -- битовая маска активных блокировок
- `estop` -- аварийная остановка (кнопка E-STOP)
- `filter_warn` -- предупреждение о загрязнении фильтра

### ro_plant/status/diagnostics

```json
{
  "heap_free": 120000,
  "heap_min": 80000,
  "uptime_s": 3600,
  "wdt_stale": 0,
  "stack": {
    "modbus": 2048,
    "io": 1024,
    "process": 4096,
    "watchdog": 512,
    "mqtt": 2048
  },
  "modbus": {
    "errors": [0, 0, 0, 0],
    "online": [true, true, true, false]
  }
}
```

Поля верхнего уровня:
- `heap_free` -- свободная куча (байт)
- `heap_min` -- минимум свободной кучи за все время (байт)
- `uptime_s` -- время работы контроллера (секунды)
- `wdt_stale` -- счетчик "протухших" задач watchdog

Вложенный объект `stack` (остатки стеков задач, байт):
- `modbus`, `io`, `process`, `watchdog`, `mqtt`

Вложенный объект `modbus`:
- `errors` -- массив [4] счетчиков ошибок по устройствам
- `online` -- массив [4] статусов доступности устройств

### ro_plant/alarms

```json
{
  "id": 1,
  "ts": 1700000000,
  "cat": "ALARM",
  "code": "HIGH_P1",
  "value": 12.5,
  "active": true
}
```

Поля:
- `id` -- уникальный идентификатор аварии
- `ts` -- метка времени (unix timestamp)
- `cat` -- категория: `"INFO"`, `"WARNING"`, `"ALARM"`, `"CRITICAL"`
- `code` -- строковый код аварии (до 23 символов)
- `value` -- значение параметра, вызвавшего аварию
- `active` -- авария активна (true) или квитирована (false)

## Зависимости

- **cJSON**: Парсинг JSON (компонент ESP-IDF)
- **plant_data**: Потокобезопасное хранилище данных (`plant_data_set_*`, `plant_data_lock/unlock`)
- **alarm_ring**: Кольцевой буфер аварий (`alarm_ring_push`)
- **mqtt_topics**: Определения строк MQTT-топиков
- **math.h**: Константа `NAN` для отсутствующих значений

## Связь с другими модулями

- **mqtt_app.c** вызывает `mqtt_handle_incoming()` при получении MQTT_EVENT_DATA
- Обновленные данные в **plant_data** читаются модулем UI (`ui_main.c`, экраны LVGL)
- Аварии в **alarm_ring** отображаются на экране аварий (`scr_mnemonic.c`)
- Флаги `DIRTY_*` используются UI для определения, какие элементы экрана нужно перерисовать

## Обработка ошибок

- **Ошибка парсинга JSON**: Логирование предупреждения (`ESP_LOGW`), сообщение игнорируется
- **Отсутствующие ключи**: Используются значения по умолчанию (0, false, NAN)
- **Неизвестные строковые значения перечислений**: Возвращается значение `UNKNOWN`/`NONE`
- **Переполнение буфера топика**: Топик обрезается до 127 символов
- **Потокобезопасность**: Все сеттеры `plant_data` используют внутренний мьютекс; `alarm_ring_push` также потокобезопасен

## Условная компиляция

Весь файл `mqtt_parser.c` обернут в `#ifndef LVGL_LIVE_PREVIEW` -- при сборке для предпросмотра UI модуль исключается из компиляции.
