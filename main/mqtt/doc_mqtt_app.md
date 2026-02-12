# Модуль mqtt_app -- MQTT-клиент HMI-дисплея

## Расположение файлов

- `main/mqtt/mqtt_app.c` -- реализация
- `main/mqtt/mqtt_app.h` -- заголовочный файл (публичный API)

## Назначение

Модуль управляет MQTT-клиентом HMI-дисплея: подключение к брокеру, подписка на топики, прием входящих сообщений, публикация команд управления и настроек. Является связующим звеном между сетевой подсистемой и парсером данных.

## Подробное описание

### Конфигурация клиента

| Параметр | Значение | Описание |
|----------|----------|----------|
| URI брокера | `CONFIG_MQTT_BROKER_URI` | По умолчанию `mqtt://192.168.1.1:1883` |
| Client ID | `CONFIG_MQTT_CLIENT_ID` | Идентификатор HMI-клиента |
| Last Will Topic | `ro_hmi/availability` | Топик статуса доступности |
| Last Will Message | `"offline"` | Публикуется брокером при потере связи |
| Last Will QoS | 1 | Гарантированная доставка |
| Last Will Retain | 1 | Сохраняется на брокере |
| Reconnect | 5000 мс | Интервал автопереподключения |
| Stack Size | 8192 байт | Размер стека задачи MQTT |
| Task Priority | 6 | Приоритет задачи MQTT |
| Buffer Size | 2048 байт | Размер буфера приема/передачи |

### Жизненный цикл подключения

1. `mqtt_client_start()` -- инициализация и запуск клиента
2. При подключении (MQTT_EVENT_CONNECTED):
   - Устанавливается `mqtt_connected = true` в plant_data
   - Подписка на `ro_plant/status/#` (QoS 0) -- все статусные топики
   - Подписка на `ro_plant/alarms` (QoS 1) -- аварии (с гарантией доставки)
   - Публикация `"online"` на `ro_hmi/availability` (retained)
3. При отключении (MQTT_EVENT_DISCONNECTED):
   - Устанавливается `mqtt_connected = false` в plant_data
   - Брокер публикует Last Will `"offline"` на `ro_hmi/availability`
   - Автопереподключение через 5 секунд
4. При получении данных (MQTT_EVENT_DATA):
   - Вызов `mqtt_handle_incoming()` из модуля mqtt_parser
5. `mqtt_client_stop()` -- остановка и уничтожение клиента

### QoS стратегия

- **QoS 0** для статусных топиков (`ro_plant/status/#`) -- данные обновляются часто, потеря одного сообщения не критична
- **QoS 1** для аварий (`ro_plant/alarms`) -- гарантия доставки аварийных сообщений
- **QoS 1** для команд управления и настроек -- гарантия доставки команд оператора

## Функции

### Управление клиентом

#### `esp_err_t mqtt_client_start(void)`

Инициализация и запуск MQTT-клиента. Создает клиент с заданной конфигурацией, регистрирует обработчик событий и запускает подключение к брокеру.

- **Возвращает**: `ESP_OK` при успехе, `ESP_FAIL` при ошибке инициализации

#### `void mqtt_client_stop(void)`

Корректная остановка и уничтожение MQTT-клиента. Освобождает все ресурсы.

#### `bool mqtt_client_is_connected(void)`

Потокобезопасная проверка статуса подключения через чтение `plant_data.mqtt_connected` с мьютексом (таймаут 10 мс).

- **Возвращает**: `true` если клиент подключен к брокеру

### Публикация команд управления

#### `esp_err_t mqtt_publish_mode_cmd(const char *cmd)`

Публикация команды смены режима на топик `ro_plant/command/mode`.

- **Параметры**: `cmd` -- строка режима (`"AUTO"`, `"IDLE"`, `"MANUAL"`, `"WASHING"`)
- **Формат**: Открытая строка (не JSON)

#### `esp_err_t mqtt_publish_pump_mask(uint8_t mask)`

Публикация битовой маски насосов на топик `ro_plant/command/pump`.

- **Параметры**: `mask` -- битовая маска (каждый бит = один насос)
- **Формат JSON**: `{"mask": N}`

#### `esp_err_t mqtt_publish_doser_enable(bool enabled)`

Публикация команды дозатора на топик `ro_plant/command/doser`.

- **Параметры**: `enabled` -- `true`/`false`
- **Формат JSON**: `{"enabled": true}` или `{"enabled": false}`

#### `esp_err_t mqtt_publish_heater(bool on)`

Публикация команды нагревателя на топик `ro_plant/command/heater`.

- **Параметры**: `on` -- `true`/`false`
- **Формат JSON**: `{"on": true}` или `{"on": false}`

### Публикация настроек (уставок)

#### `esp_err_t mqtt_publish_settings_pressure(const settings_pressure_t *s)`

Публикация уставок давления на топик `ro_plant/settings/pressure`.

- **Формат JSON**: `{"p1_max":10.0, "p3_max":15.0, "p4_max":20.0, "filter_dp_warn":0.5}`

#### `esp_err_t mqtt_publish_settings_doser(const settings_doser_t *s)`

Публикация настроек дозатора на топик `ro_plant/settings/doser`.

- **Формат JSON**: `{"run_time_min":30, "cycle_time_min":60}`

#### `esp_err_t mqtt_publish_settings_washing(const settings_washing_t *s)`

Публикация настроек промывки на топик `ro_plant/settings/washing`.

- **Формат JSON**: `{"target_temp_C":40.0, "max_temp_C":45.0, "t_overshoot_C":2.0, "hysteresis_C":1.0, "heat_timeout_min":60, "supply_time_min":30, "drain_time_min":15}`

#### `esp_err_t mqtt_publish_settings_timeouts(const settings_timeouts_t *s)`

Публикация таймаутов насосов на топик `ro_plant/settings/timeouts`.

- **Формат JSON**: `{"pump_confirm_ms":5000, "pump_ramp_ms":3000}`

## MQTT-топики

### Подписка (Subscribe)

| Топик | QoS | Описание |
|-------|-----|----------|
| `ro_plant/status/#` | 0 | Все статусные данные установки |
| `ro_plant/alarms` | 1 | Аварийные сообщения |

### Публикация (Publish)

| Топик | QoS | Описание |
|-------|-----|----------|
| `ro_hmi/availability` | 1 | Статус доступности HMI (`"online"`/`"offline"`) |
| `ro_plant/command/mode` | 1 | Команда смены режима |
| `ro_plant/command/pump` | 1 | Управление насосами |
| `ro_plant/command/doser` | 1 | Управление дозатором |
| `ro_plant/command/heater` | 1 | Управление нагревателем |
| `ro_plant/settings/pressure` | 1 | Уставки давления |
| `ro_plant/settings/doser` | 1 | Настройки дозатора |
| `ro_plant/settings/washing` | 1 | Настройки промывки |
| `ro_plant/settings/timeouts` | 1 | Таймауты насосов |

## Зависимости

- **ESP-IDF**: `mqtt_client.h` (ESP-MQTT), `esp_log.h`
- **Внутренние модули**:
  - `mqtt_topics.h` -- определения топиков
  - `mqtt_parser.h` -- парсер входящих сообщений (`mqtt_handle_incoming`)
  - `plant_data.h` -- общее хранилище данных (`plant_data_set_mqtt_status`, `plant_data_get`)

## Связь с другими модулями

- Запускается из `app_main.c` после успешной инициализации Ethernet (`eth_init`)
- Входящие данные передаются в `mqtt_parser` для разбора
- Команды и настройки вызываются из модуля UI (`ui_events.c`) при нажатии кнопок на экране

## Обработка ошибок

- Все функции публикации проверяют `s_client != NULL` перед отправкой, возвращая `ESP_ERR_INVALID_STATE` если клиент не инициализирован
- Возвращаемое значение `esp_mqtt_client_publish` проверяется: `>= 0` -- успех, `< 0` -- ошибка
- Ошибки MQTT (MQTT_EVENT_ERROR) логируются с типом ошибки
- Отключение от брокера обрабатывается автоматически с переподключением каждые 5 секунд

## Условная компиляция

- В `mqtt_app.c`: Весь файл обернут в `#ifndef LVGL_LIVE_PREVIEW`
- В `mqtt_app.h`: Для режима `LVGL_LIVE_PREVIEW` определяются заглушки типов (`esp_err_t = int`, `ESP_OK = 0`), что позволяет компилировать UI-код без ESP-IDF
