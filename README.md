| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# RO Plant HMI Display (ro_hmi)

HMI-дисплей на базе Waveshare ESP32-P4-NANO для управления и мониторинга установки обратного осмоса (RO).
Взаимодействует с основным контроллером (ESP32-S3) по MQTT через Ethernet.

## Аппаратная платформа

| Компонент | Описание |
|-----------|----------|
| Плата | Waveshare ESP32-P4-NANO |
| MCU | ESP32-P4 (dual-core RISC-V, 400 MHz) |
| Дисплей | 10.1" IPS, MIPI DSI 2-lane (контроллер JD9365), нативное разрешение 800x1280 (портрет), программный поворот 270° в 1280x800 (ландшафт) |
| MCU дисплея | I2C адрес 0x45 — преинициализация (reg 0x95, 0x96) выполняется внутри компонента waveshare автоматически |
| Тачскрин | Ёмкостный GT911, I2C 0x5D, режим POLLING (прерывания вызывают WDT crash — GT911 удерживает INT LOW после инициализации) |
| Сеть | Ethernet RMII (IP101 PHY, RST = GPIO51) |
| Память | 16 MB Flash QIO + 32 MB PSRAM 200 MHz |
| Аудио | ES8311 на I2C 0x18 (не используется в текущей прошивке) |

## Карта GPIO

| GPIO | Назначение |
|------|------------|
| 7 | I2C SDA (общая шина: MCU 0x45, GT911 0x5D, ES8311 0x18) |
| 8 | I2C SCL |
| 21 | GT911 INT (не используется — режим polling) |
| 22 | GT911 RST |
| 23 | Подсветка LCD (LEDC PWM, 20 кГц) |
| 27 | Аппаратный сброс LCD-панели |
| 31 | Ethernet MDC |
| 51 | Ethernet PHY RST |
| 52 | Ethernet MDIO |

## Шина I2C

Общая шина I2C (I2C_NUM_1, 400 кГц) инициализируется в `board_i2c_init()` до инициализации дисплея и тачскрина.
Три устройства на шине: 0x18 (ES8311), 0x45 (MCU дисплея), 0x5D (GT911).

## Архитектура ПО

```
app_main.c                  -- точка входа
board/
  board_i2c.c/.h            -- общая шина I2C (master, 400 кГц)
  board.h                   -- аппаратные константы (GPIO, разрешения, частоты)
  display_init.c/.h         -- дисплей MIPI DSI (JD9365), LVGL, подсветка
  touch_init.c/.h           -- тачскрин GT911 (I2C, режим polling)
net/
  eth_init.c/.h             -- Ethernet (EMAC + IP101 PHY, DHCP)
mqtt/
  mqtt_app.c/.h             -- MQTT-клиент
  mqtt_parser.c/.h          -- парсер JSON-сообщений
  mqtt_topics.h             -- определения MQTT-топиков
data/
  plant_data.c/.h           -- модель данных установки
  alarm_ring.c/.h           -- кольцевой буфер аварий
i18n/
  lang.c/.h                 -- модуль локализации
  lang_strings.h            -- ключи строк
  lang_ru.c, lang_en.c      -- переводы RU/EN
ui/
  ui_main.c/.h              -- главный UI: навигация, верхняя панель, переключение экранов
  ui_common.c/.h            -- общие виджеты
  ui_events.c/.h            -- обработчики UI-событий
  ui_theme.c/.h             -- цвета, шрифты, константы макета
  lvgl_preview.c            -- заглушки для LVGL preview mode (без железа)
  screens/
    scr_mnemonic.c/.h       -- мнемосхема
    scr_parameters.c/.h     -- таблица аналоговых параметров
    scr_alarms.c/.h         -- активные аварии + история
    scr_washing.c/.h        -- управление промывкой мембран
    scr_settings.c/.h       -- настройки/уставки
    scr_diagnostics.c/.h    -- диагностика
fonts/
  Montserrat_*.c            -- растеризованные шрифты Montserrat (12-36pt, 4bpp)
```

## Последовательность инициализации

```
app_main()
  ├── nvs_flash_init()
  ├── board_i2c_init()              // Общая шина I2C (SDA=7, SCL=8, 400 кГц)
  │     └── board_i2c_scan()        // i2cdetect-сканирование (0x18, 0x45, 0x5D)
  ├── display_init()
  │     ├── backlight_init()        // LEDC PWM GPIO23
  │     ├── LDO ch3 → 2.5V         // Питание MIPI DSI PHY
  │     ├── MIPI DSI bus (2 lanes)
  │     ├── JD9365 panel init       // Преинит MCU 0x45 внутри компонента
  │     ├── panel_reset()           // Аппаратный сброс через GPIO27
  │     ├── LVGL port init
  │     ├── lvgl_port_add_disp_dsi  // avoid_tearing=false, sw_rotate=true
  │     ├── rotation 270°           // Ландшафт 1280x800
  │     └── backlight 100%
  ├── touch_init(disp)
  │     ├── board_i2c_get_bus()     // Повторное использование общей шины
  │     ├── GT911 I2C panel IO      // Адрес 0x5D, 400 кГц
  │     ├── GT911 touch create      // Polling mode (int_gpio=NC)
  │     └── lvgl_port_add_touch()   // Регистрация в LVGL (LV_INDEV_MODE_TIMER)
  ├── plant_data_init()
  ├── alarm_ring_init()
  ├── lang_init(LANG_RU)
  ├── ui_init(disp)
  ├── eth_init()                    // Ethernet: IP101 PHY, RST=GPIO51
  ├── eth_wait_for_ip(10s)
  └── mqtt_client_start()
```

## Ключевые технические решения

### Дисплей

- Нативное разрешение: 800x1280 (портрет), MIPI DSI 2-lane, 1500 Мбит/с на линию
- Программный поворот: `LV_DISPLAY_ROTATION_270` → ландшафт 1280x800
- `avoid_tearing = false`: sw_rotate несовместим с avoid_tearing в esp_lvgl_port. При `avoid_tearing=true` LVGL рисует напрямую в DSI frame buffer, но буфер программного поворота отключается от DSI pipeline → белый экран
- Буферы: double_buffer в SPIRAM + DMA, 800x80 = 64000 пикселей на буфер
- `num_fbs = 2`: необходимо для двойной буферизации

### Тачскрин

- GT911 в режиме polling (`int_gpio_num = GPIO_NUM_NC`): в режиме прерываний GT911 удерживает INT LOW после инициализации, вызывая шторм NEGEDGE-прерываний, который блокирует FreeRTOS tick и приводит к WDT crash
- Без `swap_xy`: GT911 на данной плате уже сконфигурирован в ландшафтной ориентации (raw X = 0..1280, raw Y = 0..800)
- Без `driver_data`: процедура инициализации адреса (переключение INT+RST GPIO) пропущена для избежания конфликта с режимом прерываний

### I2C

- Компонент `waveshare/esp_lcd_jd9365_10_1` использует legacy `i2c_bus` API, тогда как board_i2c и touch используют новый `i2c_master` API — `CONFIG_I2C_SKIP_LEGACY_CONFLICT_CHECK=y`
- `board_i2c_init()` вызывается до `display_init()`, чтобы компонент waveshare обнаружил существующую шину через `i2c_master_get_bus_handle()`

## MQTT-топики

| Направление | Топик | Описание |
|-------------|-------|----------|
| HMI <- PLC | `ro_plant/status/#` | Состояние, I/O, датчики, телеметрия |
| HMI <- PLC | `ro_plant/alarms` | Аварийные сообщения (JSON) |
| HMI -> PLC | `ro_plant/command/{mode,pump,doser,heater}` | Команды управления |
| HMI -> PLC | `ro_plant/settings/{pressure,doser,washing,timeouts}` | Уставки |
| HMI (LWT) | `ro_hmi/availability` | online / offline (retained) |

Подробное описание MQTT: см. `docs/mqtt_config.md` и `docs/hmi_data_config.md`.

## Зависимости

- **ESP-IDF** >= 5.3.0
- **LVGL** ~9.2 (через esp_lvgl_port ^2)
- **esp_lcd_touch** ^1.1
- **esp_lcd_touch_gt911** ^1 (из ESP Component Registry)
- **waveshare/esp_lcd_jd9365_10_1** (из ESP Component Registry)

## Сборка и прошивка

**Рекомендуемый способ — через `build.sh`** (автоматически переключает target на esp32p4 при необходимости):

```bash
./build.sh                # собрать
./build.sh flash          # собрать и залить
./build.sh flash monitor  # собрать, залить, открыть монитор
./build.sh clean          # полная очистка build/, sdkconfig
```

Ручной способ (если ESP-IDF уже настроен под esp32p4):

```bash
idf.py build
idf.py -p PORT flash monitor
```

⚠️ **При первой сборке после клонирования или после смены ветки** — обязательно
`./build.sh` (или `idf.py set-target esp32p4` вручную). Без этого ESP-IDF
может использовать default target (esp32) и сборка упадёт на
`fatal error: esp_lcd_mipi_dsi.h: No such file or directory` — DSI-источники
в `esp_lcd` собираются только под `CONFIG_SOC_MIPI_DSI_SUPPORTED=y`,
который активен только для esp32p4.

Для выхода из монитора: `Ctrl-]`

## Таблица разделов (OTA-ready, 16 МБ flash)

| Раздел | Тип | Смещение | Размер | Назначение |
|--------|-----|----------|--------|---|
| nvs | data/nvs | 0x9000 | 24 KB | системный NVS (network creds, OTA state) |
| otadata | data/ota | 0xF000 | 8 KB | состояние OTA (текущий слот) |
| phy_init | data/phy | 0x11000 | 4 KB | RF калибровка |
| ota_0 | app | 0x20000 | 3 MB | приложение слот A |
| ota_1 | app | 0x320000 | 3 MB | приложение слот B (rollback) |
| nvs_storage | data/nvs | 0x620000 | 1 MB | пользовательские данные (уставки, alarm history) |
| coredump | data/coredump | 0x720000 | 64 KB | core dump при crash |

Свободно для будущих расширений: ~9 МБ.

## Ключевые настройки (sdkconfig.defaults)

- PSRAM 200 MHz + XIP, L2 cache 256 KB / 128B line
- FreeRTOS 1000 Hz, main task stack 10 KB
- LVGL color depth 16-bit (RGB565), Montserrat 14
- MQTT buffer 4096 bytes (для diagnostic-payload без фрагментации)
- Task WDT timeout 30s, LVGL task в WDT registered
- Bootloader rollback enabled (для безопасного OTA)
- Ethernet PHY RST = GPIO51 (Waveshare ESP32-P4-NANO)
- I2C legacy conflict check disabled
- Target = esp32p4 (закреплён в `CMakeLists.txt` через `set(IDF_TARGET)`)
