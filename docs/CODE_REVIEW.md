# Код-ревью HMI ESP32-P4 — список проблем

> **Дата:** 2026-05-10. Ветка: `waveshare-esp32p4-nano` @ `f8cf536`.
>
> **Объём:** ревью покрывает MQTT/data слой, board (HAL дисплея/тача/I2C), net (Ethernet), app_main, build-конфигурацию.
>
> **НЕ покрывает:** UI-слой (`main/ui/`) — пользователь явно отметил, что графический интерфейс будет переделываться, поэтому ревью UI-кода нерационально.
>
> Проведён 3 параллельными агентами. Объединено и приоритезировано.

## Сводная таблица

| Слой | 🔴 CRITICAL | 🟠 HIGH | 🟡 MEDIUM | 🟢 LOW | Итого |
|---|---:|---:|---:|---:|---:|
| MQTT / data | 5 | 9 | 10 | 8 | 32 |
| Board / net (HAL) | 5 | 6 | 8 | 6 | 25 |
| App_main / build | 1 | 4 | 5 | 6 | 16 |
| **Итого** | **11** | **19** | **23** | **20** | **73** |

---

## 🔝 Top-15 must-fix (по приоритету и импакту)

### Блокеры (чинить до заливки на стенд)

1. ✅ **`sdkconfig` устарел — WDT timeout = 5с вместо 30с** (`sdkconfig:1458`)
   `sdkconfig` сгенерирован под IDF 5.3.4 до того как `sdkconfig.defaults` стал требовать 30с WDT. UI-init не успеет, → паника при первом старте на железе.
   **Fix:** удалить `sdkconfig`, добавить в `.gitignore`, регенерировать через `idf.py reconfigure`.
   **DONE (commit 6aae3cb)**

2. ✅ **`portMAX_DELAY` в 15+ сеттерах `plant_data_set_*`** (`plant_data.c`)
   Прямо нарушает правило проекта (CLAUDE.md контроллера). MQTT-task зависнет на mutex если UI-task застрянет в LVGL.
   **Fix:** заменить на `pdMS_TO_TICKS(50)`, на timeout — drop + лог.
   **DONE (commit 6aae3cb)**

3. ✅ **`portMAX_DELAY` во всех 7 функциях `alarm_ring_*`** (`alarm_ring.c:59,82,118,140,158,177,195`)
   То же что #2. UI читает 32 записи под mutex → MQTT блокируется на рендере экрана аварий.
   **Fix:** конечный таймаут + счётчик `dropped_alarms`.
   **DONE (commit 6aae3cb)**

4. ✅ **`display_init` падает → `app_main` молча `return` → "зомби" прошивка** (`app_main.c:79`)
   FreeRTOS scheduler работает, idle-task пинает HW WDT, но устройство не делает ничего видимого. Оператор не поймёт что произошло.
   **Fix:** `esp_restart()` после `ESP_LOGE` или emergency-режим с MQTT-уведомлением.
   **DONE (commit 6aae3cb)**

5. ✅ **`ESP_ERROR_CHECK` в HAL → bootloop вместо degraded mode** (`display_init.c:86,95,102,108,135-138`)
   Любой сбой DSI/LDO → `abort()`, противоречит «UI некритичен». На плате без панели — вечный bootloop.
   **Fix:** заменить на `goto cleanup` + `return NULL`.
   **DONE (commit 6aae3cb)**

### Высокий приоритет

6. ✅ **Ресурс-leak в `display_init.c`: LDO/LEDC/LVGL_port не освобождаются на cleanup** (`display_init.c:90-199`)
   На любой ошибке инициализации остаётся включённое питание PHY DSI без шанса вернуть, подвисший LVGL-таймер. Bootloop с прогрессирующим перегревом LDO.
   **Fix:** дополнить `cleanup:` всеми ресурсами в обратном порядке.
   **DONE (commit 6aae3cb)**

7. ✅ **Race в `parse_alarm` + неустановка `dirty_flags` в fallback** (`mqtt_parser.c:466-473`)
   В fallback-ветке (mutex timeout) аларм добавлен в ring, но UI не подхватит. Пропущенная критическая авария на UI на несколько секунд. Также nested locking двух мьютексов.
   **Fix:** atomic-bool «alarms_dirty», push **вне** plant_data-лока. Зафиксировать порядок мьютексов в комментарии.
   **DONE (commit 6aae3cb)**

8. ✅ **Нет `controller_online` флага в plant_data → HMI показывает stale-данные** (`mqtt_parser.c:510`)
   При получении `availability=offline` только лог. UI не показывает оператору что данные устарели → видит «AUTO RUNNING» с актуальными цифрами хотя контроллер мёртв.
   **Fix:** `bool controller_online` в plant_data, выставлять из availability-handler.
   **DONE (commit 6aae3cb)**

9. ✅ **`parse_state` затирает state на невалидном JSON** (`mqtt_parser.c:171-178`)
   `parse_state_enum(NULL)` → `PLANT_STATE_UNKNOWN` → перезаписывает существующее состояние одним битым сообщением.
   **Fix:** обновлять только присутствующие поля либо отвергать весь объект.
   **DONE (commit 6aae3cb)**

10. ✅ **`waveshare/esp_lcd_jd9365_10_1: "*"` — версия незакреплена** (`idf_component.yml:6`)
    Любое ломающее обновление → белый экран при пересборке.
    **Fix:** `"^1.0"` или `"~1.0.4"`.
    **DONE (commit 6aae3cb)**

11. ⏳ **NVS save в UI-task без таймаута** (`plant_data.c:368-378`)
    `nvs_set_blob + commit` блокируют 10-100 мс на flash write. UI зафризится после нажатия «Применить».
    **Fix:** очередь write-задач в low-priority worker.
    **OPEN**

12. ✅ **`ETH_GOT_IP_BIT` не сбрасывается на link-down** (`eth_init.c:69-72`)
    `eth_wait_for_ip()` после потери и повторного получения IP вернёт ESP_OK мгновенно. Тихий баг, проявится при повторном использовании.
    **Fix:** `xEventGroupClearBits(..., ETH_GOT_IP_BIT)` в `IP_EVENT_ETH_LOST_IP`.
    **DONE (commit 6aae3cb)**

### Средний приоритет, но влияет на качество

13. ✅ **`MQTT_EVENT_DATA` фрагменты молча отбрасываются** (`mqtt_app.c:70-74`)
    Payload > 2 КБ → молчаливая потеря. `diagnostics` со всеми массивами modbus + stack может пухнуть.
    **Fix:** увеличить `buffer.size` до 4 КБ; либо собирать фрагменты по `msg_id`.
    **DONE (commit 6aae3cb)**

14. ⏳ **Ресурс-leak в `eth_init.c:135-150`** — каскадные ошибки не освобождают MAC/PHY/netif
    **Fix:** `goto cleanup` + явное освобождение, либо `ESP_ERROR_CHECK` (для встроенного PHY приемлемо).
    **OPEN**

15. ✅ **`DIRTY_STATE` неправильно ставится в save_settings** (`plant_data.c:384,395,406,417`)
    Все 4 save-функции ставят `DIRTY_STATE` вместо `DIRTY_SETTINGS` → UI перерисовывает не те виджеты.
    **Fix:** добавить `DIRTY_SETTINGS` в `plant_data.h`.
    **DONE (commit 6aae3cb)**

---

## 🔴 CRITICAL — все 11 проблем

### MQTT/data (5)

| № | Где | Что |
|---|---|---|
| C1 | `mqtt_app.c:178/195/213/230/250/271/292/317` | Race на `s_client` без synchronization (TOCTOU между UI-publish и stop) |
| C2 | `plant_data.c` 15+ мест | `portMAX_DELAY` в сеттерах, **см. Top-2** |
| C3 | `mqtt_parser.c:466-473` | Race + неустановка dirty_flags в `parse_alarm`, **см. Top-7** |
| C4 | `alarm_ring.c:59,82,118,140,158,177,195` | `portMAX_DELAY` в 7 функциях, **см. Top-3** |
| C5 | `alarm_ring_get_history` | memcpy 32 записей под mutex в UI-task → блокирует MQTT |

### Board / Net (5)

| № | Где | Что |
|---|---|---|
| C6 | `display_init.c:90` | LDO handle утекает на стек, нет геттера/деинита |
| C7 | `display_init.c:199` | `cleanup:` оставляет LDO/LEDC/LVGL_port, **см. Top-6** |
| C8 | `display_init.c:86-138` | `ESP_ERROR_CHECK` в HAL → bootloop, **см. Top-5** |
| C9 | `eth_init.c:135-150` | Ресурс-leak MAC/PHY/netif на cascading errors, **см. Top-14** |
| C10 | `eth_init.c:69-72` | `ETH_GOT_IP_BIT` не сбрасывается, **см. Top-12** |

### App_main / Build (1)

| № | Где | Что |
|---|---|---|
| C11 | `sdkconfig:1458` | Stale, WDT=5s вместо 30s, **см. Top-1** |

---

## 🟠 HIGH (19) — по группам

### Многопоточность / синхронизация (3)
- H1 `mqtt_app.c:42-89`: подписка только на CONNECTED, без `MQTT_EVENT_SUBSCRIBED` → молча подключён без данных при ACL deny
- H2 `mqtt_app.c:70-74`: фрагменты молча отбрасываются (см. Top-13)
- H_e `eth_init.c:51,71`: `plant_data_set_mqtt_status` из event-loop callback нарушает правило «HAL не зависит от services»

### State / data integrity (3)
- H3 нет `controller_online` (см. Top-8)
- H4 NVS-операции синхронные в UI-task (см. Top-11)
- H9 `DIRTY_STATE` в save_settings (см. Top-15)

### Архитектура HAL (5)
- H_b `board_i2c.c:58-67`: `enable_internal_pullup=true` на 400 кГц с 3 устройствами — risky если внешних pull-up нет
- H_c `board_i2c.c:17,77`: нет mutex на `s_i2c_bus`, публичный API без барьеров
- H_d `display_init.c:151,164,165`: `buff_spiram=true` + `buff_dma=true` для DSI на ESP32-P4 — cache coherency может быть не гарантирована
- H_d2 `display_init.c:71`: draw-buffer 800×80 — недостаточен для ландшафта 1280×80 при rotation 270
- H_e2 `eth_init.c:97-98`: `esp_event_loop_create_default()` без проверки на `ESP_ERR_INVALID_STATE`

### Build (3)
- H10 версия Waveshare driver `*` (см. Top-10)
- H_b3 `idf: ">=5.3.0"` вместо `">=5.4,<6"` LTS
- H_b4 partitions.csv: 13 МБ простаивает, NVS=24 КБ — мало, нет OTA

### Безопасность / устойчивость (5)
- H5 `MQTT_TOPIC_BUF_SIZE = 128` magic local define
- H6 `int valueint` для uint32-полей в cJSON
- H7 нет `_Static_assert` против самого длинного топика
- H8 нет backoff при reconnect (5с фикс)
- H_b5 `display_init` failure → "зомби" (см. Top-4)

---

## 🟡 MEDIUM (23) — кратко

**Архитектурные:**
- M1 `mqtt_client_is_connected` берёт mutex ради bool — лучше atomic
- M2 `last_msg_time_us` обновляется но никем не читается (мёртвый код)
- M3 `parse_state` без sanity-check на сочетания
- M4 `cJSON_ParseWithLength` без защиты от bomb-payload
- M5 magic 4 для modbus устройств в `parse_diagnostics` (нет `_Static_assert`)
- M9 `parse_state` затирает на невалидном JSON (см. Top-9)
- M10 `LVGL_LIVE_PREVIEW` обёртки во всех .c — лучше через CMake target

**HAL:**
- M_b1 `LEDC_TIMER_1` для подсветки + хардкод 1023 max_duty
- M_b2 подсветка на 100% сразу после init панели (видно мусор RAM до первого кадра)
- M_b3 `BOARD_TOUCH_INT_GPIO` определён но плавающий в polling-режиме
- M_b4 `pdTRUE/xWaitForAllBits` для одного бита — лишнее
- M_b5 нет `_Static_assert` на размер draw-буфера

**Build:**
- M_c1 нет ручной регистрации в WDT (только idle-task)
- M_c2 `lang_init(LANG_RU)` имя default_lang вводит в заблуждение
- M_c3 `lang.c` не потокобезопасен
- M_c4 client_id `ro_hmi` фиксирован — две панели не уживутся
- M_c5 README не отражает текущее состояние

**Прочее:**
- M6 `entry.code` truncation хрупкая через `strncpy` (лучше `snprintf`)
- M7 MQTT credentials anonymous в Kconfig дефолтах
- M8 wildcard subscribe `ro_plant/status/#` без счётчика unknown topics
- M_b6 i2c-scan на каждом boot блокирует app_main на ~6 сек
- M_b7 `phy_addr=1` хардкод вместо AUTODETECT
- M_b8 нет `eth_deinit()` симметричной к `eth_init()`

---

## 🟢 LOW (20) — мелочи

Стилистика, magic numbers без последствий, лишние логи, неоптимальные комментарии. Не угрожают работе. Список целиком в архивных отчётах агентов; в этом сводном — пропущено.

---

## 📦 Структурное предложение

Из ревью app_main/build (агент 3): **вынести в `components/`**:

```
components/
  bsp_waveshare_p4_nano/    # board_i2c, display_init, touch_init, eth_init
                            # → переиспользуется как BSP в любом проекте на этой плате
  i18n/                     # generic, не привязан к RO
  fonts/                    # generic
main/
  app_main.c                # минимум
  mqtt/                     # specific to RO
  data/                     # specific to RO
  ui/                       # specific to RO
```

Преимущества:
- Инкрементальные сборки (CMake кеш по компонентам)
- Возможность вынести `bsp_waveshare_p4_nano` в отдельный repo
- Упростит host-тесты `data/` и `i18n/` (по образцу контроллера)

**Папка `yaml/` неясна:** не упоминается в `main/CMakeLists.txt`, скорее всего это design-time generator-source. Стоит явно отметить в README. Иначе `yaml.zip` (19 КБ) и копии в `yaml/` — мёртвый груз в репо.

---

## ✅ Положительные стороны (не сломать при доработках)

### Архитектура
- **Снимок-паттерн** для writer→reader (`parse_power` + `plant_data_set_power_meter`) — корректный snapshot-pattern
- **NaN-семантика** «нет данных» во всех float-полях — соответствует backend-контракту
- **LWT с QoS=1 + retain=1** для HMI availability — настроен правильно
- **`cJSON_Delete` всегда после маршрутизации** (single exit point) — без утечек
- **Отдельный мьютекс для `alarm_ring`** (а не переиспользование plant_data) — правильный design
- **`parse_diagnostics` гибко обрабатывает modbus-массив** — robust

### HAL
- **`board_i2c` выделен из `touch_init`** — правильное решение под shared bus
- **`int_gpio_num = GPIO_NUM_NC` + комментарий про WDT crash** в touch_init — отличная защита от регрессии
- **`avoid_tearing=false` с подробным разбором** в display_init — спасает от white-screen-bug, не трогать
- **`sw_rotate` + `LV_DISPLAY_ROTATION_270`** для JD9365 — единственно возможное решение
- **`eth_wait_for_ip` через EventGroup** — идиоматично, поток-безопасно
- **`eth_init` корректно возвращает `esp_err_t`** — UI работает без сети

### App_main / build
- **Чистая последовательность инициализации** с подробными русскими комментариями
- **Graceful degradation**: critical (display) vs non-critical (touch, ethernet) — присутствует
- **I2C-bus init ДО display** для shared handle через `i2c_master_get_bus_handle()` — нетривиально и правильно
- **`lang.c` мини, строки во flash** — память используется правильно
- **`CONFIG_I2C_SKIP_LEGACY_CONFLICT_CHECK=y`** + комментарий — хороший warning trail
- **`doc_*.md` рядом с кодом** — отличная практика, обновляются вместе с кодом

---

## 🛠 Рекомендуемый план работ

### ✅ Спринт 1 (блокеры — 1 день) — DONE 2026-05-10
- **Top-1..5** — отказы при первом запуске на железе, нельзя заливать без них
- **COMPLETED via commit 6aae3cb** тремя параллельными агентами

### ✅ Спринт 2 (data integrity — 2 дня) — DONE 2026-05-10
- **Top-6..10, 12, 13, 15** → DONE (commit 6aae3cb)
- **Top-11** (NVS worker queue) → DONE (commit abd429b) — новые `nvs_worker.{c,h}` + `plant_data_internal.h`
- **Top-14** (eth_init resource leak) → DONE (commit abd429b) — `goto cleanup` + `eth_deinit()`

### 🟨 Спринт 3 (зрелость — 3-5 дней) — PARTIAL DONE
- ✅ H_c (i2c bus mutex), H_e (HAL не зависит от services), H_e2 (event_loop INVALID_STATE), H7 (_Static_assert на топики), M_c2/M_c3 (lang thread-safety) → DONE (commit abd429b)
- 🟡 Остаётся секция HIGH: H1 (subscribe failed handling), H2 (фрагменты), H_b (i2c pullup), H_d (DSI cache coherency), H_d2 (draw-buffer для ландшафта), H_b4 (partitions), H5/H6/H8 — мелкие
- 📦 Структурное предложение: вынести board → `components/bsp_waveshare_p4_nano/`

### Backlog (когда переделается UI)
- Все MEDIUM/LOW
- TLS + auth для MQTT (M7) — при выходе за trusted LAN

---

## История

- **2026-05-10**: первое полное ревью HMI после интеграции с актуальным API контроллера. UI-слой не покрыт по запросу пользователя (предстоит переделка).
- **2026-05-10 — commit 6aae3cb**: Sprint 1, закрыто 13 из 15 Top-проблем (✅ Top-1..10, 12, 13, 15) тремя параллельными агентами. Остаются Top-11 и Top-14.
- **2026-05-10 — commit abd429b**: Sprint 2, закрыты остатки Top-15 (Top-11 NVS worker queue, Top-14 eth_init resource leak) + 5 проблем категории HIGH/MEDIUM (H_c, H_e, H_e2, H7, M_c2/M_c3) тремя параллельными агентами. **Top-15 ПОЛНОСТЬЮ ЗАКРЫТ.** Из 19 HIGH остаётся ~9 (общая категория HIGH без Top-15).
