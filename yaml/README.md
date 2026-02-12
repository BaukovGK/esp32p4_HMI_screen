# RO Plant Display — ESPHome LVGL Configuration

ESPHome-конфигурация для дисплея управления установкой обратного осмоса (RO Plant).
Воспроизводит интерфейс оригинальной прошивки ESP32-P4 (LVGL 9 + MIPI DSI 1280×800).

---

## Структура файлов

```
yaml/
├── ro_plant_display.yaml    # Главный файл конфигурации
├── ro_plant_display_hw.yaml # Дисплей, тачскрин, SPI/I2C
├── ro_plant_screens.yaml    # 6 экранов (LVGL-виджеты)
├── ro_plant_sensors.yaml    # MQTT-сенсоры, кнопки, настройки
├── ro_plant_colors.yaml     # Палитра цветов (34 цвета)
├── ro_plant_fonts.yaml      # Шрифты Roboto 12–36px + кириллица
├── secrets.yaml             # WiFi / API ключи (не коммитить!)
└── README.md                # Этот файл
```

---

## Требования

- **ESPHome** 2024.6+ (с поддержкой LVGL-компонента)
- **ESP32-S3** или **ESP32-P4** плата с достаточным объёмом PSRAM
- **Дисплей** 1280×800 с MIPI DSI или SPI интерфейсом
- **Тачскрин** GT911 / GSL3680 (I2C)
- **MQTT-брокер** (Mosquitto, EMQX и т.д.)
- **PLC/контроллер** — источник данных, публикующий в MQTT-топики `ro_plant/...`

---

## Быстрый старт

### 1. Настройка секретов

Отредактируйте `secrets.yaml`:

```yaml
wifi_ssid: "MyNetwork"
wifi_password: "MyPassword"
api_key: "generated-api-key-here"
```

### 2. Настройка MQTT-брокера

В `ro_plant_display.yaml` укажите адрес брокера:

```yaml
substitutions:
  mqtt_broker: "192.168.1.100"
  mqtt_port: "1883"
  mqtt_user: ""
  mqtt_password: ""
```

### 3. Настройка оборудования

В `ro_plant_display_hw.yaml` скорректируйте пины под вашу плату:

```yaml
i2c:
  sda: GPIO8    # ← ваш пин SDA
  scl: GPIO9    # ← ваш пин SCL

touchscreen:
  - platform: gt911
    interrupt_pin: GPIO4
    reset_pin: GPIO5
```

### 4. Компиляция и прошивка

```bash
esphome compile yaml/ro_plant_display.yaml
esphome upload yaml/ro_plant_display.yaml
```

Или через ESPHome Dashboard:

```bash
esphome dashboard yaml/
```

---

## Экраны

### 1. Мнемосхема (P&ID)

Технологическая схема установки с визуализацией в реальном времени.

```
[Исходный]──P1──P2──[M1]──[Промежут.]──P3──[M2]──[Пермеат]
   бак        ↑   ↑         бак                      бак
            Нагр. Доз.
```

| Элемент | Визуализация |
|---------|-------------|
| Баки (3 шт.) | Прямоугольники 80×220px, уровень заливки по DI-датчикам |
| Насосы (3 шт.) | Круги 60px: серый=OFF, жёлтый=STARTING, зелёный=RUNNING, красный=FAULT |
| Мембраны (2 шт.) | Блоки 100×80px с подписями M1/M2 |
| Нагреватель, дозатор | Круги 50px с той же цветовой логикой |
| Датчики | T, P1–P4, Q1–Q4, S1–S3 — зелёный текст при нормальных значениях |
| Панель телеметрии | Recovery %, Filter dP, текущий режим |
| Кнопки | СТАРТ АВТО / СТОП / РУЧНОЙ / ПРОМЫВКА / СБРОС АВАРИИ |

### 2. Параметры

Таблица всех измеренных и расчётных значений.

| Секция | Параметры |
|--------|-----------|
| Давление | P1–P4 (bar) |
| Температура | T (°C) |
| Расход | Q1–Q4 (m³/h) + накопленный объём (m³) |
| Проводимость | S1–S3 (µS/cm) + температура датчика |
| Расчётные | Перепад фильтра, Recovery системы, Селективность 1–2 |

### 3. Промывка

7-фазный степпер цикла промывки.

```
[1]──[2]──[3]──[4]──[5]──[6]──[7]
Ожид. Нагрев Ожид.  Подача Ожид. Слив  Готово
нагр.        подачи        слива
```

- Завершённые фазы — зелёные
- Текущая фаза — голубая (accent)
- Будущие — тёмно-синие
- Большой индикатор температуры: текущая / целевая
- Кнопки: СТАРТ (до промывки), ПОДТВЕРДИТЬ (в фазах ожидания), СТОП

### 4. Аварии

| Блок | Описание |
|------|----------|
| Активные аварии | Скроллируемый список с цветовой категорией (CRITICAL/ALARM/WARNING/INFO) |
| История | Архив аварий с таймстампами |
| Сброс | Кнопка СБРОС АВАРИИ (видна только при состоянии FAULT) |

### 5. Настройки

4 вкладки с параметрами + цифровая клавиатура справа.

| Вкладка | Параметры |
|---------|-----------|
| Давление | P1/P3/P4 Max (bar), dP фильтра предупреждение |
| Дозатор | Время работы (мин), Время цикла (мин) |
| Промывка | Целевая/макс. температура, перерегулирование, гистерезис, таймауты |
| Таймауты | Подтверждение насоса (мс), Разгон насоса (мс) |

Цифровая клавиатура:

```
[ 7 ] [ 8 ] [ 9 ] [DEL]
[ 4 ] [ 5 ] [ 6 ] [ AC]
[ 1 ] [ 2 ] [ 3 ] [ . ]
[     0     ] [   OK   ]
```

### 6. Диагностика

| Панель | Содержимое |
|--------|-----------|
| Система | Heap Free/Min, Uptime, MQTT статус |
| Modbus-устройства | 4 устройства: статус ONLINE/OFFLINE, счётчик ошибок |
| Стеки задач | Modbus, IO, Process, Watchdog, MQTT — свободный стек (bytes) |

---

## MQTT-топики

### Подписки (чтение данных от PLC)

| Топик | Данные |
|-------|--------|
| `ro_plant/status/state` | Состояние: IDLE / AUTO / WASHING / MANUAL / FAULT |
| `ro_plant/status/io` | DI/DO биты (JSON) |
| `ro_plant/status/analog/p0..p3` | Давление P1–P4 (bar) |
| `ro_plant/status/analog/t` | Температура (°C) |
| `ro_plant/status/flow/q0..q3` | Расход Q1–Q4 (m³/h) |
| `ro_plant/status/flow/q0_vol..q3_vol` | Объём Q1–Q4 (m³) |
| `ro_plant/status/conductivity/s0..s2` | Проводимость S1–S3 (µS/cm) |
| `ro_plant/status/conductivity/s0_temp..s2_temp` | Температура S1–S3 (°C) |
| `ro_plant/status/telemetry/recovery` | Recovery (%) |
| `ro_plant/status/telemetry/filter_dp` | Перепад фильтра (bar) |
| `ro_plant/status/telemetry/selectivity1..2` | Селективность (%) |
| `ro_plant/status/telemetry/recovery_sys` | Recovery системы (%) |
| `ro_plant/status/washing/target_temp` | Целевая температура промывки (°C) |
| `ro_plant/status/diagnostics/heap_free` | Свободная heap (bytes) |
| `ro_plant/status/diagnostics/heap_min` | Минимальная heap (bytes) |
| `ro_plant/status/diagnostics/uptime` | Аптайм (секунды) |
| `ro_plant/status/diagnostics/modbus` | Статус Modbus-устройств |
| `ro_plant/alarms/active` | Текст активной аварии |
| `ro_plant/alarms/history` | История аварий |

### Публикации (команды к PLC)

| Топик | Payload | Действие |
|-------|---------|----------|
| `ro_plant/command/mode` | `start_auto` | Запуск автоматического режима |
| | `stop` | Остановка |
| | `set_manual` | Переход в ручной режим |
| | `start_washing` | Запуск промывки |
| | `confirm_wash` | Подтверждение фазы промывки |
| | `reset_fault` | Сброс аварии |
| `ro_plant/settings/pressure` | JSON: `p1_max`, `p3_max`, `p4_max`, `filter_dp_warn` |
| `ro_plant/settings/doser` | JSON: `run_time_min`, `cycle_time_min` |
| `ro_plant/settings/washing` | JSON: `target_temp`, `max_temp`, `overshoot`, `hysteresis`, ... |
| `ro_plant/settings/timeouts` | JSON: `pump_confirm_ms`, `pump_ramp_ms` |

---

## Цветовая схема

Тёмная индустриальная тема, оптимизированная для промышленных условий.

| Назначение | Цвет | HEX |
|-----------|------|-----|
| Фон основной | Тёмно-синий | `#1A1A2E` |
| Фон панелей | Синий | `#16213E` |
| Фон виджетов | Средне-синий | `#0F3460` |
| Текст основной | Светло-серый | `#E8E8E8` |
| Текст подписей | Серый | `#8899AA` |
| Значения | Ярко-зелёный | `#00FF88` |
| Акцент | Голубой | `#00B4D8` |
| Режим IDLE | Серый | `#6C757D` |
| Режим AUTO | Зелёный | `#28A745` |
| Режим WASHING | Синий | `#007BFF` |
| Режим MANUAL | Жёлтый | `#FFC107` |
| Режим FAULT | Красный | `#DC3545` |
| Оборуд. OFF | Тёмно-серый | `#4A4A4A` |
| Оборуд. STARTING | Жёлтый | `#FFD700` |
| Оборуд. RUNNING | Зелёный | `#00FF88` |
| Оборуд. FAULT | Красный | `#FF0000` |

---

## Адаптация под другой дисплей

Для дисплея меньшего разрешения (например, 480×320 SPI ILI9341):

1. В `ro_plant_display_hw.yaml` замените `display:` на:
   ```yaml
   display:
     - platform: ili9xxx
       model: ILI9341
       cs_pin: GPIO15
       dc_pin: GPIO2
       dimensions:
         width: 480
         height: 320
   ```

2. В `ro_plant_screens.yaml` пропорционально уменьшите все координаты и размеры (коэффициент ~0.375 по ширине, ~0.4 по высоте)

3. Уменьшите размеры шрифтов в `ro_plant_fonts.yaml` (12→8, 14→10, 16→12, 28→18 и т.д.)

---

## Известные ограничения

- **MIPI DSI** — нативно не поддерживается в ESPHome; требуется custom component или альтернативный дисплей
- **Динамические списки аварий** — LVGL в ESPHome не поддерживает динамическое создание виджетов из lambda; для полного функционала потребуется custom component
- **Canvas/рисование** — P&ID-диаграмма использует статичные объекты вместо canvas-рисования; для анимированных потоков воды нужен custom widget
- **Numpad интеграция** — связь клавиатуры с активным textarea требует lambda-обработчиков для каждой кнопки
- **JSON-парсинг** — если PLC отправляет составные JSON-сообщения, потребуется ArduinoJson в lambda

---

## Соответствие оригинальному коду

| Оригинал (C/LVGL9) | ESPHome YAML |
|--------------------|-------------|
| `main/ui/ui_main.c` | `ro_plant_display.yaml` + `ro_plant_screens.yaml` (навигация, alarm bar) |
| `main/ui/screens/scr_mnemonic.c` | page_mnemonic в `ro_plant_screens.yaml` |
| `main/ui/screens/scr_parameters.c` | page_parameters |
| `main/ui/screens/scr_washing.c` | page_washing |
| `main/ui/screens/scr_alarms.c` | page_alarms |
| `main/ui/screens/scr_settings.c` | page_settings |
| `main/ui/screens/scr_diagnostics.c` | page_diagnostics |
| `main/ui/ui_theme.c` | `ro_plant_colors.yaml` |
| `main/ui/ui_events.c` | on_click / lambda в `ro_plant_screens.yaml` |
| `main/data/plant_data.c` | globals + sensors в `ro_plant_sensors.yaml` |
| `main/mqtt/mqtt_app.c` | mqtt: секция в `ro_plant_display.yaml` |
| `main/mqtt/mqtt_parser.c` | on_value lambda в `ro_plant_sensors.yaml` |
