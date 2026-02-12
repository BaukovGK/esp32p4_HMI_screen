# Модуль plant_data -- Хранилище данных установки

## Файлы

| Файл | Путь | Назначение |
|------|------|------------|
| Заголовок | `main/data/plant_data.h` | Определения типов, структур, перечислений, прототипы API |
| Реализация | `main/data/plant_data.c` | Единственный экземпляр данных, мьютекс, сеттеры/геттеры |

## Назначение

Модуль `plant_data` является **центральным хранилищем состояния** установки обратного осмоса на HMI-дисплее (ESP32-P4). Хранит полный снимок данных, получаемых от основного контроллера (ESP32-S3) по MQTT:

- Состояние автоматики (IDLE / AUTO / WASHING / MANUAL / FAULT)
- Аналоговые датчики: давление (P1-P4), температура (T)
- Расходомеры (Q1-Q4) и кондуктометры (s1-s3)
- Расчётная телеметрия (перепад давления, выход пермеата, селективность)
- Дискретные входы/выходы, блокировки, дозатор, диагностика
- Локальный кэш уставок контроллера

## Структуры данных

### Перечисления

| Тип | Значения | Описание |
|-----|----------|----------|
| `plant_state_t` | IDLE, AUTO, WASHING, MANUAL, FAULT, UNKNOWN | Основное состояние установки |
| `auto_sub_state_t` | NONE, STARTING_PUMP1, RAMP, STARTING_PUMP2, FILLING_INTERM, STARTING_PUMP3, RUNNING, STOPPING | Подсостояния автоматического режима |
| `wash_sub_state_t` | NONE, WAIT_HEAT, HEATING, WAIT_SUPPLY, SUPPLY, WAIT_DRAIN, DRAIN, DONE | Подсостояния промывки |
| `doser_state_t` | OFF, RUNNING, PAUSE | Состояние дозатора антискаланта |
| `alarm_category_t` | INFO(0), WARNING(1), ALARM(2), CRITICAL(3) | Категория аварии (по возрастанию критичности) |

### Структуры датчиков

| Структура | Поля | Описание |
|-----------|------|----------|
| `analog_channel_t` | `value` (float), `fault` (bool) | Аналоговый канал. `value` = NAN при неисправности, `fault` = обрыв линии |
| `flow_channel_t` | `flow` (float, м3/ч), `volume` (float, м3), `ok` (bool) | Расходомер. Мгновенный расход + накопленный объём |
| `cond_channel_t` | `conductivity` (float, мкСм/см), `temperature` (float, C), `ok` (bool) | Кондуктометр с температурной компенсацией |

### Структуры подсистем

| Структура | Поля | Описание |
|-----------|------|----------|
| `telemetry_t` | `filter_dp`, `stage1_feed`, `recovery2`, `recovery_sys`, `sel1`, `sel2` | Расчётные параметры (вычисляет контроллер) |
| `interlocks_t` | `flags` (uint32), `estop` (bool), `filter_warn` (bool) | Блокировки: битовая маска + аварийная кнопка + фильтр |
| `doser_status_t` | `state` (doser_state_t), `enabled` (bool) | Статус дозатора |
| `diagnostics_t` | heap_free/min, uptime, stack_* (5 задач), modbus_errors/online[4], wdt_stale | Диагностика контроллера ESP32-S3 |
| `alarm_entry_t` | `id`, `ts`, `cat`, `code[24]`, `value`, `active` | Запись журнала аварий |

### Структуры уставок

| Структура | Поля | Описание |
|-----------|------|----------|
| `settings_pressure_t` | `p1_max`, `p3_max`, `p4_max`, `filter_dp_warn` | Пороги давления (бар) |
| `settings_doser_t` | `run_time_min`, `cycle_time_min` | Параметры цикла дозирования (мин) |
| `settings_washing_t` | `target_temp_C`, `max_temp_C`, `t_overshoot_C`, `hysteresis_C`, `heat_timeout_min`, `supply_time_min`, `drain_time_min` | Параметры химической промывки |
| `settings_timeouts_t` | `pump_confirm_ms`, `pump_ramp_ms` | Таймауты пуска насосов (мс) |

### Главная структура `plant_data_t`

Объединяет все данные в единую структуру:

```
plant_data_t
  +-- state, auto_sub, wash_sub, fault_flags   (состояние автоматики)
  +-- di, do_bits                               (дискретные I/O, 8 бит каждый)
  +-- pressure[4], temperature                  (аналоговые: P1-P4, T)
  +-- flow[4]                                   (расходомеры: Q1-Q4)
  +-- conductivity[3]                           (кондуктометры: s1-s3)
  +-- telemetry                                 (расчётные параметры)
  +-- interlocks, doser, diagnostics            (подсистемы)
  +-- mqtt_connected, last_msg_time_us          (статус HMI)
  +-- set_pressure, set_doser, set_washing, set_timeouts  (кэш уставок)
  +-- dirty_flags                               (битовая маска изменений)
```

## Грязные флаги (dirty flags)

Механизм оптимизации перерисовки UI. Каждый сеттер устанавливает соответствующий бит:

| Флаг | Бит | Группа данных |
|------|-----|---------------|
| `DIRTY_STATE` | 0 | Состояние автоматики, MQTT-статус |
| `DIRTY_IO` | 1 | Дискретные входы/выходы |
| `DIRTY_ANALOG` | 2 | Давление (P1-P4), температура (T) |
| `DIRTY_FLOW` | 3 | Расходомеры (Q1-Q4) |
| `DIRTY_CONDUCTIVITY` | 4 | Кондуктометры (s1-s3) |
| `DIRTY_TELEMETRY` | 5 | Расчётная телеметрия |
| `DIRTY_INTERLOCKS` | 6 | Блокировки |
| `DIRTY_DOSER` | 7 | Дозатор |
| `DIRTY_DIAGNOSTICS` | 8 | Диагностика |
| `DIRTY_ALARMS` | 9 | Журнал аварий |
| `DIRTY_ALL` | 0..9 | Все группы (0x3FF) |

UI-задача вызывает `plant_data_get_and_clear_dirty()` и перерисовывает только виджеты, соответствующие установленным битам.

## Функции API

### Инициализация

| Функция | Описание |
|---------|----------|
| `plant_data_init()` | Обнуление структуры, инициализация аналоговых каналов в NAN, заполнение уставок значениями по умолчанию, создание мьютекса. Вызывается однократно при старте до запуска задач. |

### Доступ для чтения (UI-задача)

| Функция | Описание |
|---------|----------|
| `plant_data_lock(timeout_ms)` | Захват мьютекса с таймаутом. Возвращает `true` при успехе. |
| `plant_data_unlock()` | Освобождение мьютекса. |
| `plant_data_get()` | Указатель `const` на данные. Только между lock/unlock! |
| `plant_data_get_mutable()` | Мутабельный указатель. Только между lock/unlock! |
| `plant_data_get_and_clear_dirty()` | Атомарное чтение и сброс dirty_flags. Только между lock/unlock! |

### Потокобезопасные сеттеры (MQTT-задача)

Каждый сеттер самостоятельно захватывает мьютекс с `portMAX_DELAY`, записывает данные, устанавливает dirty-бит, обновляет `last_msg_time_us` и освобождает мьютекс.

| Функция | Dirty-бит | Описание |
|---------|-----------|----------|
| `plant_data_set_state(state, asub, wsub, fault_flags)` | DIRTY_STATE | Состояние, подсостояния, флаги аварий |
| `plant_data_set_io(di, do_bits)` | DIRTY_IO | Дискретные входы/выходы |
| `plant_data_set_pressure(idx, value, fault)` | DIRTY_ANALOG | Датчик давления (idx: 0-3 = P1-P4) |
| `plant_data_set_temperature(value, fault)` | DIRTY_ANALOG | Датчик температуры |
| `plant_data_set_flow(idx, flow, volume, ok)` | DIRTY_FLOW | Расходомер (idx: 0-3 = Q1-Q4) |
| `plant_data_set_conductivity(idx, cond, temp, ok)` | DIRTY_CONDUCTIVITY | Кондуктометр (idx: 0-2 = s1-s3) |
| `plant_data_set_telemetry(tel)` | DIRTY_TELEMETRY | Расчётная телеметрия (копирование структуры) |
| `plant_data_set_interlocks(flags, estop, filter_warn)` | DIRTY_INTERLOCKS | Блокировки |
| `plant_data_set_doser(state, enabled)` | DIRTY_DOSER | Дозатор |
| `plant_data_set_diagnostics(diag)` | DIRTY_DIAGNOSTICS | Диагностика (копирование структуры) |
| `plant_data_set_mqtt_status(connected)` | DIRTY_STATE | Статус MQTT (НЕ обновляет last_msg_time_us) |

## Потокобезопасность

### Модель доступа

Система использует паттерн **один писатель -- один читатель** с мьютексом:

- **Писатель** (MQTT-задача): вызывает `plant_data_set_*()`. Каждый сеттер атомарно захватывает мьютекс, пишет, освобождает.
- **Читатель** (UI-задача): вызывает `plant_data_lock()`, читает нужные поля через `plant_data_get()`, затем `plant_data_unlock()`.

### Мьютекс

- Тип: `SemaphoreHandle_t` (FreeRTOS mutex с поддержкой инверсии приоритетов)
- Создаётся в `plant_data_init()` через `xSemaphoreCreateMutex()`
- Сеттеры используют `portMAX_DELAY` (бесконечное ожидание)
- Lock UI-стороны использует таймаут для предотвращения зависания

### Важные замечания

- `plant_data_get()` и `plant_data_get_mutable()` **не захватывают мьютекс** -- вызывающий код должен удерживать блокировку.
- `plant_data_get_and_clear_dirty()` также **не захватывает мьютекс** -- предполагается вызов внутри lock/unlock.
- Сеттеры с индексом (`set_pressure`, `set_flow`, `set_conductivity`) проверяют границы массива до захвата мьютекса.

## Значения по умолчанию

При инициализации устанавливаются следующие значения:

| Параметр | Значение |
|----------|----------|
| state | `PLANT_STATE_UNKNOWN` |
| Все аналоговые value/flow/volume/conductivity/temperature | `NAN` |
| set_pressure | p1_max=5.5, p3_max=35.0, p4_max=8.0, filter_dp_warn=1.0 |
| set_doser | run=5 мин, cycle=60 мин |
| set_washing | target=35C, max=40C, overshoot=45C, hyst=2C, heat_timeout=30мин, supply=20мин, drain=5мин |
| set_timeouts | pump_confirm=3000мс, pump_ramp=15000мс |

## Зависимости

- **FreeRTOS** (`freertos/FreeRTOS.h`, `freertos/semphr.h`): мьютекс
- **ESP-IDF** (`esp_timer.h`): метка времени `esp_timer_get_time()`
- **Стандартная библиотека** (`string.h`, `math.h`, `stdint.h`, `stdbool.h`)

### Кто зависит от этого модуля

- `alarm_ring.c/h` -- использует `alarm_entry_t`, `alarm_category_t`
- `mqtt_parser.c` -- вызывает сеттеры при разборе MQTT-сообщений
- `mqtt_app.c` -- вызывает `plant_data_set_mqtt_status()`
- `ui_main.c` / `ui_events.c` -- читает данные для отрисовки экранов

## Условная компиляция

Файл `plant_data.c` обёрнут в `#ifndef LVGL_LIVE_PREVIEW`, что позволяет исключить его из сборки при предпросмотре UI в LVGL-симуляторе (где FreeRTOS недоступен).
