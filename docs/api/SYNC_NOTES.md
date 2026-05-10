# Журнал синхронизации HMI ↔ API контроллера

> Источник истины: `docs/api/asyncapi.yaml` и `docs/api/openapi.yaml` —
> копии из `BaukovGK/esp32s3_main_controller`, ветка `dev`. Полное расхождение
> между HMI и реальной прошивкой контроллера выясняется чтением реальных
> файлов прошивки (`components/mqtt_app/mqtt_subscribe.c` и `mqtt_publish.c`),
> а не по поверхностным пунктам в чек-листах.

---

## Статус интеграции на 2026-05-10

**HMI компилируется и запускается.** В этой итерации MQTT/data слой адаптирован под актуальный API контроллера. UI-виджеты для новых данных (s4 conductivity, KWS power) — следующая итерация.

### ✅ Реализовано в HMI (P1)

| # | Что | Где | Заметка |
|---|---|---|---|
| 1 | **Conductivity 4-й канал `s4`** (концентрат) | `plant_data.h` массив 3→4; `mqtt_parser.c` ветка `s4` | Данные собираются и хранятся; UI пока показывает только s1..s3 |
| 2 | **KWS-306L** (`power/lp`, `power/hp`) | новая `power_meter_data_t`; `pumps[2]` в `plant_data_t`; setter+геттеры; парсер | UI экран «Энергопотребление» — следующая итерация |
| 3 | **Diagnostics `modbus`** — массив объектов `[{addr,errors,online},…]` (раньше HMI ожидал параллельные массивы) | `mqtt_parser.c` | Реальный bug — теперь HMI читает корректно |
| 4 | **Controller availability** (LWT топик `ro_plant/availability`) | `mqtt_app.c` подписка | Сейчас только логгируется; флаг `controller_online` в `plant_data_t` — следующая итерация |
| 5 | `MQTT_TOPIC_POWER_PREFIX` + `MQTT_TOPIC_AVAILABILITY` | `mqtt_topics.h` | |

### 🟡 Осталось (UI-уровень)

| # | Что | Приоритет |
|---|---|---|
| 1 | UI виджет σ4 на экране «Параметры»/«Мнемосхема» (циклы захардкожены `< 3`) | P1 |
| 2 | UI экран «Энергопотребление» (V/A/W/kWh/T × 2 насоса) | P1 |
| 3 | UI индикатор `controller_online` (по топику `ro_plant/availability`) | P2 |
| 4 | Новые alarm-коды (0x00C1..0x00D7) — отображаются автоматически если HMI не фильтрует по коду | P2 |

---

## Опровергнутые «расхождения» (бывший SYNC_NOTES P0)

Предыдущая версия этого файла содержала ошибочные пункты, которые **не подтвердились** при сверке с реальным кодом контроллера (`Controller/components/mqtt_app/mqtt_subscribe.c` и `mqtt_publish.c`):

| Пункт | Статус | Что на самом деле |
|---|---|---|
| ~~Регистр топиков `P1` vs `p0` etc.~~ | ✅ HMI уже корректен | Контроллер публикует `analog/P1..P4`, `flow/Q1..Q4`, `conductivity/s1..s4`, `analog/T` — HMI подписывается на эти же. |
| ~~Команды mode `AUTO/IDLE/MANUAL/WASHING`~~ | ✅ HMI уже корректен | Контроллер ожидает `start_auto / stop / start_washing / confirm_wash / set_manual / reset_fault`. См. `mqtt_subscribe.c:24-29`. asyncapi.yaml:245 описывает это правильно. |
| ~~Префикс `settings/` → `config/`~~ | ✅ HMI уже корректен | Контроллер слушает на `ro_plant/settings/#` (`mqtt_subscribe.c:196,224`). |
| ~~JSON структура: один объект на топик vs подтопики~~ | ✅ HMI уже корректен | Парсер HMI всегда работал через cJSON, разворачивая один JSON-объект из топика на поля структуры. |
| ~~Алармы: `name`/`category`/`raised_us`~~ | ✅ HMI уже корректен | Контроллер публикует `{id, ts, cat, code, value, active}` (`mqtt_publish.c:188-190`). asyncapi.yaml:572 — правильная схема. |

**Вывод:** SYNC_NOTES первой версии был сгенерирован на основе устаревших гипотез без сверки с реальной прошивкой. Не доверяй чек-листам, которые не подкреплены ссылками на конкретные строки в коде контроллера.

---

## Источник истины — машиночитаемые спеки

Все каналы, формат сообщений, QoS, retain — формализованы в `docs/api/`:
- **`asyncapi.yaml`** — 20 каналов / 23 message-схемы (MQTT)
- **`openapi.yaml`** — 18 endpoints (REST API контроллера)

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

## История изменений

- **2026-05-10**: первая итерация интеграции.
  - Скопированы `asyncapi.yaml`, `openapi.yaml`, `README.md` и 3 doc-файла из контроллера.
  - В HMI: добавлен 4-й канал conductivity (`s4`), поддержка KWS-306L (`power_meter_data_t` + парсер + подписки), исправлен парсинг массива `diagnostics.modbus`, подписка на `ro_plant/availability`.
  - SYNC_NOTES переписан после сверки с реальным кодом контроллера: убраны ложные «расхождения» по командам mode / префиксу settings / структуре JSON / схеме алармов.
