# Модуль интернационализации (i18n)

## Обзор

Модуль i18n обеспечивает мультиязычную поддержку пользовательского интерфейса HMI-панели управления установкой обратного осмоса. Поддерживаются два языка: **русский** (основной) и **английский** (вспомогательный). Переключение языка происходит в реальном времени без перезагрузки устройства.

---

## Файлы модуля

| Файл | Путь | Назначение |
|------|------|------------|
| `lang.h` | `main/i18n/lang.h` | Публичный интерфейс модуля: типы `lang_id_t`, прототипы функций, extern-объявления таблиц строк |
| `lang_strings.h` | `main/i18n/lang_strings.h` | Перечисление `str_id_t` -- идентификаторы всех локализуемых строк |
| `lang.c` | `main/i18n/lang.c` | Реализация: хранение текущего языка, выбор таблицы строк, функция `lang_str()` |
| `lang_ru.c` | `main/i18n/lang_ru.c` | Таблица строк на русском языке (UTF-8, hex-экранирование кириллицы) |
| `lang_en.c` | `main/i18n/lang_en.c` | Таблица строк на английском языке |

---

## Архитектура

### Принцип работы

Модуль построен на паттерне **таблица строк с индексным доступом**:

```
str_id_t (индекс)  -->  s_tables[текущий_язык][индекс]  -->  "Строка UTF-8"
```

1. Каждый язык представлен массивом `const char*` размером `STR_COUNT`.
2. Индексация выполняется значениями перечисления `str_id_t`.
3. Внутренний массив `s_tables[]` хранит указатели на таблицы всех языков.
4. Переменная `s_current_lang` определяет, какая таблица используется.

### Схема зависимостей

```
lang_strings.h        <-- определяет str_id_t (идентификаторы строк)
      |
      v
    lang.h             <-- включает lang_strings.h, определяет lang_id_t и API
      |
      +------+------+
      |      |      |
      v      v      v
   lang.c  lang_ru.c  lang_en.c
   (логика) (RU строки) (EN строки)
```

### Потокобезопасность

Модуль **не является потокобезопасным**. Предполагается, что все вызовы (`lang_set`, `lang_str`, `lang_get`) выполняются из одного потока -- задачи LVGL. Если требуется доступ из других потоков, необходимо добавить мьютекс.

---

## API

### `void lang_init(lang_id_t default_lang)`

Инициализация модуля. Регистрирует таблицы строк и устанавливает язык по умолчанию. Вызывается один раз в `app_main()`.

**Пример:**
```c
lang_init(LANG_RU);  // русский язык по умолчанию
```

### `void lang_set(lang_id_t lang)`

Переключение текущего языка. После вызова все последующие обращения к `lang_str()` будут возвращать строки на выбранном языке. UI-виджеты необходимо обновить вручную.

**Пример:**
```c
lang_set(LANG_EN);  // переключить на английский
```

### `lang_id_t lang_get(void)`

Возвращает идентификатор текущего активного языка.

**Пример:**
```c
if (lang_get() == LANG_RU) { /* ... */ }
```

### `const char *lang_str(str_id_t id)`

Основная функция модуля. Возвращает локализованную строку по идентификатору. При некорректном `id` или отсутствующей строке возвращает `"???"`.

**Пример:**
```c
lv_label_set_text(label, lang_str(STR_MODE_AUTO));  // "АВТО" или "AUTO"
```

---

## Перечисление str_id_t -- строковые идентификаторы

### Режимы работы установки (STR_MODE_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_MODE_IDLE` | ОЖИДАНИЕ | IDLE | Установка в режиме ожидания |
| `STR_MODE_AUTO` | АВТО | AUTO | Автоматический режим работы |
| `STR_MODE_WASHING` | ПРОМЫВКА | WASHING | Режим промывки мембран |
| `STR_MODE_MANUAL` | РУЧНОЙ | MANUAL | Ручной режим управления |
| `STR_MODE_FAULT` | АВАРИЯ | FAULT | Аварийное состояние |
| `STR_MODE_UNKNOWN` | НЕТ СВЯЗИ | NO LINK | Нет связи с контроллером |

### Подсостояния автоматического режима (STR_AUTO_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_AUTO_STARTING_PUMP1` | Пуск насоса подачи | Starting feed pump | Запуск насоса подачи исходной воды |
| `STR_AUTO_RAMP` | Разгон | Ramp-up | Набор рабочего давления |
| `STR_AUTO_STARTING_PUMP2` | Пуск насоса 1-й ст. | Starting stage 1 pump | Запуск насоса 1-й ступени ОО |
| `STR_AUTO_FILLING_INTERM` | Заполнение промбака | Filling intermediate tank | Заполнение промежуточной ёмкости |
| `STR_AUTO_STARTING_PUMP3` | Пуск насоса 2-й ст. | Starting stage 2 pump | Запуск насоса 2-й ступени ОО |
| `STR_AUTO_RUNNING` | Работа | Running | Штатная работа установки |
| `STR_AUTO_STOPPING` | Останов | Stopping | Процедура остановки |

### Подсостояния промывки (STR_WASH_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_WASH_WAIT_HEAT` | Подготовка к нагреву | Prepare for heating | Ожидание готовности нагревателя |
| `STR_WASH_HEATING` | Нагрев | Heating | Нагрев промывочного раствора |
| `STR_WASH_WAIT_SUPPLY` | Подготовка к подаче | Prepare for supply | Подготовка к подаче раствора |
| `STR_WASH_SUPPLY` | Подача | Supply | Подача раствора через мембраны |
| `STR_WASH_WAIT_DRAIN` | Подготовка к сливу | Prepare for drain | Подготовка к сливу |
| `STR_WASH_DRAIN` | Слив | Drain | Слив промывочного раствора |
| `STR_WASH_DONE` | Завершено | Done | Промывка завершена |

### Навигация (STR_NAV_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_NAV_MNEMONIC` | Схема | P&ID | Мнемосхема установки |
| `STR_NAV_PARAMETERS` | Парам. | Params | Технологические параметры |
| `STR_NAV_WASHING` | Промыв. | Washing | Экран управления промывкой |
| `STR_NAV_ALARMS` | Аварии | Alarms | Журнал аварийных событий |
| `STR_NAV_SETTINGS` | Настр. | Settings | Экран настроек |
| `STR_NAV_DIAGNOSTICS` | Диагн. | Diag | Диагностика контроллера |

### Кнопки (STR_BTN_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_BTN_START_AUTO` | СТАРТ АВТО | START AUTO | Запуск автоматического режима |
| `STR_BTN_STOP` | СТОП | STOP | Остановка установки |
| `STR_BTN_MANUAL` | РУЧНОЙ | MANUAL | Переход в ручной режим |
| `STR_BTN_START_WASHING` | ПРОМЫВКА | WASHING | Запуск промывки |
| `STR_BTN_CONFIRM` | ПОДТВЕРДИТЬ | CONFIRM | Подтверждение действия |
| `STR_BTN_RESET_FAULT` | СБРОС | RESET | Сброс аварии |
| `STR_BTN_APPLY` | ПРИМЕНИТЬ | APPLY | Применение настроек |
| `STR_BTN_YES` | ДА | YES | Кнопка "Да" в диалоге |
| `STR_BTN_NO` | НЕТ | NO | Кнопка "Нет" в диалоге |

### Метки оборудования (STR_LBL_* -- equipment)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_LBL_FEED_PUMP` | Насос подачи | Feed Pump | Насос подачи исходной воды |
| `STR_LBL_STAGE1_PUMP` | Насос 1-й ст. | Stage 1 Pump | Насос 1-й ступени ОО |
| `STR_LBL_STAGE2_PUMP` | Насос 2-й ст. | Stage 2 Pump | Насос 2-й ступени ОО |
| `STR_LBL_HEATER` | Нагреватель | Heater | Нагреватель промывочного раствора |
| `STR_LBL_DOSER` | Дозатор | Doser | Дозатор антискаланта |
| `STR_LBL_SOURCE_TANK` | Исх. ёмкость | Source Tank | Ёмкость исходной воды |
| `STR_LBL_INTERM_TANK` | Пром. ёмкость | Interm. Tank | Промежуточная ёмкость |
| `STR_LBL_PERMEATE_TANK` | Пермеатная | Permeate Tank | Ёмкость пермеата |
| `STR_LBL_MEMBRANE_1` | Мембрана 1 | Membrane 1 | Мембранный элемент 1 |
| `STR_LBL_MEMBRANE_2` | Мембрана 2 | Membrane 2 | Мембранный элемент 2 |
| `STR_LBL_FILTER` | Фильтр | Filter | Механический фильтр |

### Метки параметров (STR_LBL_* -- parameters)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_LBL_PRESSURE` | Давление | Pressure | Давление в системе |
| `STR_LBL_TEMPERATURE` | Температура | Temperature | Температура воды/раствора |
| `STR_LBL_FLOW_RATE` | Расход | Flow rate | Объёмный расход |
| `STR_LBL_VOLUME` | Объём | Volume | Накопленный объём |
| `STR_LBL_CONDUCTIVITY` | УЭП | Conductivity | Удельная электропроводность |
| `STR_LBL_RECOVERY` | Извлечение | Recovery | Степень извлечения пермеата |
| `STR_LBL_SELECTIVITY` | Селективность | Selectivity | Селективность мембран |
| `STR_LBL_FILTER_DP` | Перепад фильтра | Filter dP | Перепад давления на фильтре |
| `STR_LBL_STAGE1_FEED` | Подача 1-й ст. | Stage 1 feed | Подача на 1-ю ступень |

### Коды аварий (STR_ALARM_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_ALARM_ESTOP` | АВАРИЙНЫЙ СТОП | EMERGENCY STOP | Нажата кнопка аварийного останова |
| `STR_ALARM_SOURCE_EMPTY` | Источник пуст | Source tank empty | Исходная ёмкость пуста |
| `STR_ALARM_INTERM_EMPTY` | Промбак пуст | Intermediate tank empty | Промежуточная ёмкость пуста |
| `STR_ALARM_P1_HIGH` | Давление P1 высокое | P1 pressure high | Превышение давления на датчике P1 |
| `STR_ALARM_P3_HIGH` | Давление P3 высокое | P3 pressure high | Превышение давления на датчике P3 |
| `STR_ALARM_P4_HIGH` | Давление P4 высокое | P4 pressure high | Превышение давления на датчике P4 |
| `STR_ALARM_T_HIGH` | Перегрев | Overtemperature | Превышение допустимой температуры |
| `STR_ALARM_FILTER_DP` | Засорение фильтра | Filter clogged | Высокий перепад давления на фильтре |
| `STR_ALARM_PUMP1_TIMEOUT` | Нет подтв. насоса подачи | Feed pump timeout | Таймаут подтверждения пуска |
| `STR_ALARM_PUMP2_TIMEOUT` | Нет подтв. насоса 1-й ст. | Stage 1 pump timeout | Таймаут подтверждения пуска |
| `STR_ALARM_PUMP3_TIMEOUT` | Нет подтв. насоса 2-й ст. | Stage 2 pump timeout | Таймаут подтверждения пуска |
| `STR_ALARM_SENSOR_FAULT` | Обрыв датчика | Sensor fault | Неисправность или обрыв датчика |
| `STR_ALARM_MQTT_DISCONNECT` | MQTT отключён | MQTT disconnected | Потеря связи с MQTT-брокером |
| `STR_ALARM_MODBUS_OFFLINE` | Modbus оффлайн | Modbus offline | Контроллер не отвечает по Modbus |

### Разделы настроек (STR_SET_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_SET_PRESSURE` | Давление | Pressure | Уставки давления |
| `STR_SET_DOSER` | Дозатор | Doser | Параметры дозирования |
| `STR_SET_WASHING` | Промывка | Washing | Параметры промывки |
| `STR_SET_TIMEOUTS` | Таймауты | Timeouts | Таймауты и задержки |
| `STR_SET_MQTT` | MQTT | MQTT | Настройки MQTT-соединения |

### Диагностика (STR_DIAG_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_DIAG_HEAP_FREE` | Своб. память | Free heap | Свободная память (heap) |
| `STR_DIAG_HEAP_MIN` | Мин. память | Min heap | Минимальная свободная память за сессию |
| `STR_DIAG_UPTIME` | Время работы | Uptime | Время с момента включения |
| `STR_DIAG_MQTT_STATUS` | Статус MQTT | MQTT status | Текущее состояние MQTT |
| `STR_DIAG_MODBUS` | Modbus | Modbus | Состояние шины Modbus |

### Статусы (STR_STATUS_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_STATUS_ONLINE` | Онлайн | Online | Устройство в сети |
| `STR_STATUS_OFFLINE` | Оффлайн | Offline | Устройство не в сети |
| `STR_STATUS_OK` | Норма | OK | Нормальное состояние |
| `STR_STATUS_FAULT` | АВАРИЯ | FAULT | Аварийное состояние |
| `STR_STATUS_SENSOR_BREAK` | ОБРЫВ | BREAK | Обрыв датчика |
| `STR_STATUS_NO_DATA` | Нет данных | No data | Нет данных от контроллера |

### Диалог подтверждения (STR_CONFIRM_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_CONFIRM_TITLE` | Подтверждение | Confirmation | Заголовок диалогового окна |
| `STR_CONFIRM_START_AUTO` | Запустить авторежим? | Start auto mode? | Подтверждение пуска |
| `STR_CONFIRM_STOP` | Остановить установку? | Stop the plant? | Подтверждение останова |
| `STR_CONFIRM_MANUAL` | Перейти в ручной режим? | Switch to manual mode? | Подтверждение перехода |
| `STR_CONFIRM_WASHING` | Запустить промывку? | Start washing? | Подтверждение промывки |

### Единицы измерения (STR_UNIT_*)

| Идентификатор | RU | EN | Описание |
|---|---|---|---|
| `STR_UNIT_BAR` | бар | bar | Давление |
| `STR_UNIT_CELSIUS` | °C | °C | Температура |
| `STR_UNIT_M3H` | м³/ч | m³/h | Расход |
| `STR_UNIT_M3` | м³ | m³ | Объём |
| `STR_UNIT_USCM` | мкСм/см | uS/cm | Удельная электропроводность |
| `STR_UNIT_PERCENT` | % | % | Проценты |
| `STR_UNIT_SECONDS` | с | s | Секунды |
| `STR_UNIT_MINUTES` | мин | min | Минуты |
| `STR_UNIT_BYTES` | байт | B | Байты (диагностика памяти) |

---

## Механизм переключения языка

Переключение языка реализовано в `main/ui/ui_common.c` через кнопку на панели навигации:

```c
// Переключение языка по нажатию кнопки (ui_common.c)
lang_set(lang_get() == LANG_RU ? LANG_EN : LANG_RU);
// Обновление метки кнопки
lv_label_set_text(label, lang_get() == LANG_RU ? "RU" : "EN");
```

После вызова `lang_set()` модуль i18n запоминает новый язык, но **UI-виджеты не обновляются автоматически**. Код UI должен явно перечитать строки через `lang_str()` и вызвать `lv_label_set_text()` для каждого виджета.

Инициализация модуля выполняется в `app_main()`:

```c
lang_init(LANG_RU);  // русский по умолчанию
```

---

## Зависимости и связи с UI-модулями

Модуль i18n используется следующими файлами UI:

| Файл | Что использует |
|------|----------------|
| `main/ui/screens/scr_mnemonic.c` | Метки оборудования, режимы, кнопки |
| `main/ui/screens/scr_parameters.c` | Метки параметров, единицы измерения |
| `main/ui/screens/scr_washing.c` | Подсостояния промывки, кнопки |
| `main/ui/screens/scr_alarms.c` | Коды аварий, статусы |
| `main/ui/screens/scr_settings.c` | Разделы настроек, кнопки |
| `main/ui/screens/scr_diagnostics.c` | Диагностические метки, статусы |
| `main/ui/ui_common.c` | Навигация, переключение языка |
| `main/ui/ui_events.c` | Диалоги подтверждения |
| `main/app_main.c` | Инициализация (`lang_init`) |

---

## Руководство разработчика

### Добавление новой строки

1. **Добавить идентификатор** в `lang_strings.h`:
   ```c
   /* В соответствующую группу, ПЕРЕД STR_COUNT */
   STR_LBL_NEW_SENSOR,  // Описание нового датчика
   ```

2. **Добавить русский перевод** в `lang_ru.c`:
   ```c
   [STR_LBL_NEW_SENSOR] = "\xD0\x94\xD0\xB0\xD1\x82\xD1\x87\xD0\xB8\xD0\xBA",  /* Датчик */
   ```

3. **Добавить английский перевод** в `lang_en.c`:
   ```c
   [STR_LBL_NEW_SENSOR] = "Sensor",
   ```

4. **Использовать в UI-коде**:
   ```c
   lv_label_set_text(label, lang_str(STR_LBL_NEW_SENSOR));
   ```

> **Важно:** порядок элементов в `str_id_t` определяет числовой индекс. Новые элементы добавляются в конец соответствующей группы, но обязательно перед `STR_COUNT`. Не менять порядок существующих элементов.

### Добавление нового языка

1. **Добавить идентификатор языка** в `lang.h`:
   ```c
   typedef enum {
       LANG_EN = 0,
       LANG_RU,
       LANG_DE,       // <-- новый язык
       LANG_COUNT,
   } lang_id_t;
   ```

2. **Создать файл таблицы строк** `lang_de.c`:
   ```c
   #include "lang.h"
   const char *lang_de_strings[STR_COUNT] = {
       [STR_MODE_IDLE] = "STANDBY",
       /* ... все STR_COUNT строк ... */
   };
   ```

3. **Объявить extern-массив** в `lang.h`:
   ```c
   extern const char *lang_de_strings[];
   ```

4. **Зарегистрировать таблицу** в `lang_init()` (`lang.c`):
   ```c
   s_tables[LANG_DE] = lang_de_strings;
   ```

5. **Добавить файл в сборку** -- убедиться, что `lang_de.c` включён в `CMakeLists.txt`.

### Кодирование кириллицы

Русские строки хранятся в hex-экранированном UTF-8 для совместимости с любыми редакторами и компиляторами. Каждый кириллический символ занимает 2 байта:

```
А = \xD0\x90    Б = \xD0\x91    В = \xD0\x92    ...
а = \xD0\xB0    б = \xD0\xB1    в = \xD0\xB2    ...
```

Комментарий с читаемым текстом всегда добавляется в конце строки:
```c
[STR_MODE_IDLE] = "\xD0\x9E\xD0\x96\xD0\x98\xD0\x94\xD0\x90\xD0\x9D\xD0\x98\xD0\x95",  /* ОЖИДАНИЕ */
```
