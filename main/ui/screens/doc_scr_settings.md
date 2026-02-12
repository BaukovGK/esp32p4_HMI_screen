# Экран "Настройки" (scr_settings)

## Файлы
- `main/ui/screens/scr_settings.h` -- заголовок (API)
- `main/ui/screens/scr_settings.c` -- реализация

## Назначение
Редактирование уставок установки обратного осмоса.
Оператор выбирает текстовое поле касанием, вводит значение с экранной клавиатуры
и нажимает "Применить" для отправки уставок на контроллер ESP32-S3 по MQTT.

## Компоновка экрана (1280 x 700 px)

```
+--------------------------------------------------+------------------+
|  [Давление] [Дозатор] [Промывка] [Таймауты]     |                  |
|                                                   |  [7] [8] [9] [Del]
|  P1 max (bar)        [__12.00__]                 |  [4] [5] [6] [AC]|
|  P3 max (bar)        [__15.00__]                 |  [1] [2] [3] [.] |
|  P4 max (bar)        [___6.00__]                 |  [   0   ] [ OK ]|
|  Filter dP warn (bar)[___0.50__]                 |                  |
|                                                   |                  |
|  [Применить]                                      |                  |
|                                                   |                  |
|  800 px                                           |  420 px          |
+--------------------------------------------------+------------------+
```

## Иерархия виджетов

```
cont (1280x700, COLOR_BG_DARK)
  +-- tabview (800x700, слева)
  |     +-- tab_bar (кнопки вкладок)
  |     +-- tab "Давление" (FLEX_COLUMN)
  |     |     +-- field: "P1 max (bar)" + ta_p1_max
  |     |     +-- field: "P3 max (bar)" + ta_p3_max
  |     |     +-- field: "P4 max (bar)" + ta_p4_max
  |     |     +-- field: "Filter dP warn (bar)" + ta_filter_dp_warn
  |     |     +-- btn "Применить" -> evt_apply_pressure
  |     +-- tab "Дозатор" (FLEX_COLUMN)
  |     |     +-- field: "Run time (min)" + ta_run_time
  |     |     +-- field: "Cycle time (min)" + ta_cycle_time
  |     |     +-- btn "Применить" -> evt_apply_doser
  |     +-- tab "Промывка" (FLEX_COLUMN)
  |     |     +-- field: "Target temp" + ta_target_temp
  |     |     +-- field: "Max temp" + ta_max_temp
  |     |     +-- field: "Overshoot" + ta_overshoot
  |     |     +-- field: "Hysteresis" + ta_hysteresis
  |     |     +-- field: "Heat timeout (min)" + ta_heat_timeout
  |     |     +-- field: "Supply time (min)" + ta_supply_time
  |     |     +-- field: "Drain time (min)" + ta_drain_time
  |     |     +-- btn "Применить" -> evt_apply_washing
  |     +-- tab "Таймауты" (FLEX_COLUMN)
  |           +-- field: "Pump confirm (ms)" + ta_pump_confirm
  |           +-- field: "Pump ramp (ms)" + ta_pump_ramp
  |           +-- btn "Применить" -> evt_apply_timeouts
  +-- numpad (420x390, x=830, y=150)
        +-- btn "7".."9", "Del"
        +-- btn "4".."6", "AC"
        +-- btn "1".."3", "."
        +-- btn "0" (double-wide), "OK" (double-wide)
```

## Функции

| Функция | Описание |
|---|---|
| `scr_settings_create(parent)` | Создаёт вкладки + numpad |
| `scr_settings_update(cont, data)` | Заполняет пустые поля из кэша уставок |
| `make_field(parent, label, init)` | Строка: подпись + textarea |
| `make_apply_btn(tab, cb, ud)` | Кнопка "Применить" |
| `read_ta_float(ta)` / `read_ta_int(ta)` | Чтение значения из textarea |
| `ta_select_cb(e)` | Подсветка выбранного поля для numpad |
| `evt_apply_pressure(e)` | Отправка уставок давления по MQTT |
| `evt_apply_doser(e)` | Отправка уставок дозатора |
| `evt_apply_washing(e)` | Отправка уставок промывки |
| `evt_apply_timeouts(e)` | Отправка уставок таймаутов |
| `create_numpad(parent, x, y)` | Построение цифровой клавиатуры |
| `numpad_digit_cb` / `numpad_del_cb` / `numpad_ac_cb` / `numpad_enter_cb` | Обработчики кнопок numpad |

## Источники данных (plant_data_t)

Уставки загружаются один раз (при пустом текстовом поле) из кэша:

| Поле plant_data_t | Textarea | Ед.изм. |
|---|---|---|
| `set_pressure.p1_max` | ta_p1_max | bar |
| `set_pressure.p3_max` | ta_p3_max | bar |
| `set_pressure.p4_max` | ta_p4_max | bar |
| `set_pressure.filter_dp_warn` | ta_filter_dp_warn | bar |
| `set_doser.run_time_min` | ta_run_time | мин |
| `set_doser.cycle_time_min` | ta_cycle_time | мин |
| `set_washing.target_temp_C` | ta_target_temp | C |
| `set_washing.max_temp_C` | ta_max_temp | C |
| `set_washing.t_overshoot_C` | ta_overshoot | C |
| `set_washing.hysteresis_C` | ta_hysteresis | C |
| `set_washing.heat_timeout_min` | ta_heat_timeout | мин |
| `set_washing.supply_time_min` | ta_supply_time | мин |
| `set_washing.drain_time_min` | ta_drain_time | мин |
| `set_timeouts.pump_confirm_ms` | ta_pump_confirm | мс |
| `set_timeouts.pump_ramp_ms` | ta_pump_ramp | мс |

## Взаимодействие пользователя

1. Оператор переключает вкладку (касание заголовка)
2. Касается текстового поля -- оно подсвечивается зелёной рамкой
3. Вводит значение с numpad (цифры, точка, Del, AC)
4. Нажимает OK для завершения ввода
5. Нажимает "Применить" -- данные отправляются по MQTT

| Кнопка "Применить" | Функция MQTT |
|---|---|
| Давление | `mqtt_publish_settings_pressure(&s)` |
| Дозатор | `mqtt_publish_settings_doser(&s)` |
| Промывка | `mqtt_publish_settings_washing(&s)` |
| Таймауты | `mqtt_publish_settings_timeouts(&s)` |

## Механизм обновления
- `scr_settings_update()` вызывается периодически, но только для загрузки начальных значений
- Если textarea уже содержит текст (strlen > 0), значение НЕ перезаписывается
- Это предотвращает потерю пользовательского ввода

## Зависимости
- `ui_theme.h`, `ui_common.h`, `ui_events.h`, `ui_fonts.h`, `lang.h`
- `mqtt_app.h` -- `mqtt_publish_settings_*()`
- `plant_data.h` -- settings_pressure_t, settings_doser_t, settings_washing_t, settings_timeouts_t
