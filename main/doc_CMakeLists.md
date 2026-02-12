# CMakeLists.txt

**Путь:** `main/CMakeLists.txt`

**Назначение:** Конфигурация сборки главного компонента (main) прошивки HMI-дисплея в системе ESP-IDF CMake.

---

## Описание

Файл `CMakeLists.txt` в директории `main/` определяет состав исходных файлов, пути поиска заголовков и внешние зависимости (компоненты ESP-IDF) для главного компонента проекта. Это стандартный конфигурационный файл системы сборки ESP-IDF, основанной на CMake.

---

## Исходные файлы (MAIN_SRCS)

Проект состоит из следующих модулей:

### Точка входа
| Файл | Описание |
|------|----------|
| `app_main.c` | Точка входа, последовательная инициализация всех подсистем |

### Аппаратная абстракция (board/)
| Файл | Описание |
|------|----------|
| `board/display_init.c` | Инициализация MIPI DSI дисплея (JD9365) + LVGL |
| `board/touch_init.c` | Инициализация сенсорного контроллера GSL3680 (I2C) + LVGL |

### Хранилище данных (data/)
| Файл | Описание |
|------|----------|
| `data/plant_data.c` | Хранилище параметров установки обратного осмоса |
| `data/alarm_ring.c` | Кольцевой буфер аварийных событий |

### Сеть (net/)
| Файл | Описание |
|------|----------|
| `net/eth_init.c` | Инициализация Ethernet-соединения |

### MQTT (mqtt/)
| Файл | Описание |
|------|----------|
| `mqtt/mqtt_app.c` | MQTT-клиент: подключение, подписка, публикация |
| `mqtt/mqtt_parser.c` | Парсинг JSON-сообщений из MQTT-топиков |

### Пользовательский интерфейс (ui/)
| Файл | Описание |
|------|----------|
| `ui/ui_main.c` | Главный модуль UI: создание экранов, навигация |
| `ui/ui_theme.c` | Тема оформления (цвета, стили) |
| `ui/ui_common.c` | Общие UI-компоненты (панели, кнопки, индикаторы) |
| `ui/ui_events.c` | Обработка событий UI (нажатия, обновления данных) |
| `ui/lvgl_preview.c` | Заглушка для режима предпросмотра LVGL на ПК |

### Экраны (ui/screens/)
| Файл | Описание |
|------|----------|
| `ui/screens/scr_mnemonic.c` | Экран мнемосхемы (технологическая схема установки) |
| `ui/screens/scr_parameters.c` | Экран параметров (давления, расходы, TDS, температура) |
| `ui/screens/scr_washing.c` | Экран управления промывками мембран |
| `ui/screens/scr_alarms.c` | Экран аварий (текущие и история) |
| `ui/screens/scr_settings.c` | Экран настроек (уставки, сеть, яркость) |
| `ui/screens/scr_diagnostics.c` | Экран диагностики (состояние связи, версии) |

### Интернационализация (i18n/)
| Файл | Описание |
|------|----------|
| `i18n/lang.c` | Менеджер языков, выбор текущего языка |
| `i18n/lang_en.c` | Строковые ресурсы — английский |
| `i18n/lang_ru.c` | Строковые ресурсы — русский |

### Шрифты (fonts/)
| Файл | Описание |
|------|----------|
| `fonts/Montserrat_12_4.c` | Шрифт Montserrat 12px, 4bpp |
| `fonts/Montserrat_14_4.c` | Шрифт Montserrat 14px, 4bpp |
| `fonts/Montserrat_16_4.c` | Шрифт Montserrat 16px, 4bpp |
| `fonts/Montserrat_18_4.c` | Шрифт Montserrat 18px, 4bpp |
| `fonts/Montserrat_20_4.c` | Шрифт Montserrat 20px, 4bpp |
| `fonts/Montserrat_22_4.c` | Шрифт Montserrat 22px, 4bpp |
| `fonts/Montserrat_24_4.c` | Шрифт Montserrat 24px, 4bpp |
| `fonts/Montserrat_28_4.c` | Шрифт Montserrat 28px, 4bpp |
| `fonts/Montserrat_36_4.c` | Шрифт Montserrat 36px, 4bpp |

---

## Пути поиска заголовков (MAIN_INCLUDES)

| Путь | Содержимое |
|------|-----------|
| `.` | `app_main.c` и общие заголовки |
| `board` | `board.h`, `display_init.h`, `touch_init.h` |
| `data` | `plant_data.h`, `alarm_ring.h` |
| `net` | `eth_init.h` |
| `mqtt` | `mqtt_app.h`, `mqtt_parser.h` |
| `ui` | `ui_main.h`, `ui_theme.h`, `ui_common.h`, `ui_events.h` |
| `ui/screens` | Заголовки отдельных экранов |
| `i18n` | `lang.h` |
| `fonts` | Заголовки шрифтов Montserrat |

---

## Зависимости (REQUIRES)

Компонент `main` зависит от следующих компонентов ESP-IDF:

| Компонент | Назначение |
|-----------|-----------|
| `esp_lcd` | Базовый API для LCD-панелей (panel ops, I/O) |
| `esp_lcd_jd9365` | Драйвер LCD-контроллера JD9365 (MIPI DSI) |
| `esp_lcd_touch_gsl3680` | Драйвер сенсорного контроллера GSL3680 (I2C) |
| `driver` | Драйверы периферии (GPIO, LEDC, I2C) |
| `esp_eth` | Ethernet-стек ESP-IDF |
| `nvs_flash` | Энергонезависимое хранилище (NVS) |
| `esp_event` | Система событий ESP-IDF (для Ethernet, MQTT) |
| `esp_netif` | Сетевой интерфейс (TCP/IP стек) |
| `mqtt` | MQTT-клиент ESP-IDF |
| `json` | Библиотека парсинга JSON (cJSON) |
| `esp_timer` | Высокоточные таймеры (для LVGL tick) |

---

## Архитектура модулей

```
main/
 +-- app_main.c              (точка входа)
 +-- board/                   (аппаратная абстракция)
 |     +-- board.h            (константы)
 |     +-- display_init.c/h   (дисплей)
 |     +-- touch_init.c/h     (тач)
 +-- data/                    (хранилище данных)
 |     +-- plant_data.c/h     (данные установки)
 |     +-- alarm_ring.c/h     (буфер аварий)
 +-- net/                     (сеть)
 |     +-- eth_init.c/h       (Ethernet)
 +-- mqtt/                    (MQTT)
 |     +-- mqtt_app.c/h       (клиент)
 |     +-- mqtt_parser.c/h    (парсер JSON)
 +-- ui/                      (пользовательский интерфейс)
 |     +-- ui_main.c/h        (главный модуль)
 |     +-- ui_theme.c/h       (тема)
 |     +-- ui_common.c/h      (общие компоненты)
 |     +-- ui_events.c/h      (обработка событий)
 |     +-- screens/           (экраны)
 +-- i18n/                    (интернационализация)
 |     +-- lang.c/h           (менеджер)
 |     +-- lang_en.c, lang_ru.c
 +-- fonts/                   (шрифты LVGL)
       +-- Montserrat_*.c     (12..36px, 4bpp)
```
