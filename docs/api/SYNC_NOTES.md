# Расхождения HMI vs API контроллера

> Сравнение текущего кода HMI (`main/mqtt/mqtt_topics.h`, `yaml/ro_plant_sensors.yaml`) с актуальным API контроллера (`docs/api/asyncapi.yaml`, скопировано из `BaukovGK/esp32s3_main_controller` на 2026-05-10).

## TL;DR

HMI был разработан до серии расширений API контроллера. Чтобы HMI корректно работал с актуальной прошивкой ESP32-S3, нужно устранить **7 категорий расхождений**:

| # | Категория | Серьёзность | Где править |
|---|---|---|---|
| 1 | Регистр имён топиков (`P1` vs `p0`, `Q1` vs `q0`, `s1` vs `s0`, `T` vs `t`) | 🔴 P0 | `yaml/ro_plant_sensors.yaml` |
| 2 | Команды mode (`AUTO` vs `start_auto`, `IDLE` vs `stop`, ...) | 🔴 P0 | `yaml/ro_plant_sensors.yaml`, кнопки UI |
| 3 | Структура JSON: один объект на топик vs отдельные подтопики на поле | 🟠 P0 | `main/mqtt/mqtt_parser.c` |
| 4 | 4-й канал conductivity `s4` (концентрат) | 🔴 P1 | sensors.yaml + plant_data.h + UI |
| 5 | KWS-306L (`power/lp`, `power/hp`) — полностью новое | 🔴 P1 | весь HMI: data + parser + UI экран |
| 6 | Префикс настроек: `settings/` vs `config/` | 🟠 P1 | mqtt_topics.h |
| 7 | Алармы: `alarms/active` + `alarms/history` vs единый `ro_plant/alarms` | 🟡 P2 | mqtt_parser, alarm_ring |

---

## Маппинг старое → новое (P0)

### 1. Регистр имён каналов

```yaml
# БЫЛО:                         СТАНЕТ:
analog/p0..p3                 → analog/P1..P4
analog/t                      → analog/T
flow/q0..q3                   → flow/Q1..Q4
conductivity/s0..s2           → conductivity/s1..s4   (s4 НОВЫЙ — концентрат)
```

### 2. Команды mode

```yaml
# Контроллер слушает ro_plant/command/mode и принимает только эти 4 значения:
"AUTO"     # вместо "start_auto"
"IDLE"     # вместо "stop"
"MANUAL"   # вместо "set_manual"
"WASHING"  # вместо "start_washing"

# "confirm_wash", "reset_fault" — отдельные каналы, см. asyncapi.yaml
```

### 3. JSON структура (НЕ отдельные подтопики)

Контроллер публикует **один JSON-объект на топик**, не множество подтопиков:

```
ro_plant/status/conductivity/s1
  → {"conductivity": 50.2, "temperature": 25.3, "ok": true}
ro_plant/status/flow/Q1
  → {"flow": 1.523, "volume": 234.5, "ok": true}
ro_plant/status/telemetry
  → {"filter_dp":0.45, "stage1_feed":1.5, "recovery2":75.3, "recovery_sys":50.1, "sel1":98.5, "sel2":99.1}
ro_plant/status/diagnostics
  → {"heap_free":145236, "heap_min":102544, "uptime_s":3600, "task_stack":[...], "modbus":{...}}
ro_plant/status/power/{lp,hp}
  → {"voltage":230.5, "current":5.2, "power":1205, "energy":12.4, "temperature":45, "online":true}
```

Топики типа `flow/q0_vol`, `conductivity/s0_temp`, `telemetry/recovery`, `diagnostics/heap_free` **не существуют** — нужно парсить JSON (`cJSON` в HMI уже подключен).

---

## Полный API — `asyncapi.yaml` и `openapi.yaml`

Все каналы, формат сообщений, QoS, retain — формализованы в:
- **`docs/api/asyncapi.yaml`** — 20 каналов / 23 message-схемы (MQTT)
- **`docs/api/openapi.yaml`** — 18 endpoints (REST API контроллера; HMI обычно не использует, но если нужно — есть)

Просмотр в браузере:
```bash
# AsyncAPI Studio (онлайн): https://studio.asyncapi.com/ → Import → asyncapi.yaml
# Swagger UI (онлайн):       https://editor.swagger.io/  → File → Import → openapi.yaml
```

Локальная валидация:
```bash
npx @asyncapi/cli validate docs/api/asyncapi.yaml
npx @apidevtools/swagger-cli validate docs/api/openapi.yaml
```

---

## Новые alarm-коды (приходят в `ro_plant/alarms`)

Расширение списка кодов. Полный enum — в `asyncapi.yaml::AlarmCode`. Новые с прошлой синхронизации:

| Hex | Имя | Категория |
|---|---|---|
| 0x00C1 | PUMP_LP_NO_CURRENT | CRITICAL |
| 0x00C2 | PUMP_HP_NO_CURRENT | CRITICAL |
| 0x00C3 | PUMP_LP_OVERTEMP | CRITICAL |
| 0x00C4 | PUMP_HP_OVERTEMP | CRITICAL |
| 0x00C5 | KWS_VOLTAGE_OOR | ALARM |
| 0x00C6 | KWS_OFFLINE | WARNING |
| 0x00D0..0x00D7 | DEV_CHECK / AI / SL21 / URZH / KWS RANGE_OOR | ALARM (диагностика) |

Структура сообщения:
```json
{"code": 193, "name": "PUMP_LP_NO_CURRENT", "category": "CRITICAL",
 "data": 0.05, "raised_us": 1234567890, "active": true}
```

---

## Чек-лист интеграции

- [ ] **P0** Регистр имён каналов в `yaml/ro_plant_sensors.yaml`
- [ ] **P0** Payload команд mode (4 значения вместо 6 кастомных)
- [ ] **P0** Парсер MQTT — читать JSON-объект на топик
- [ ] **P1** 4-й канал conductivity (`s4` = концентрат)
- [ ] **P1** Топики `power/lp` и `power/hp` + UI «Энергопотребление»
- [ ] **P1** Префикс `settings/` → `config/` (либо настройки в контроллер не доходят)
- [ ] **P2** Алармы — единый топик `ro_plant/alarms` (без подтопиков active/history)
- [ ] **P2** Новые alarm-коды (0x00C1..0x00D7) в alarm_ring и UI

После каждого шага — проверить через `mosquitto_sub -t 'ro_plant/#' -v` что HMI получает данные на ожидаемых топиках.

---

## История

- **2026-05-10**: первая синхронизация API в HMI-репо. Скопированы `asyncapi.yaml`, `openapi.yaml`, `README.md` + 3 doc-файла из контроллера. Создан этот SYNC_NOTES.md.
