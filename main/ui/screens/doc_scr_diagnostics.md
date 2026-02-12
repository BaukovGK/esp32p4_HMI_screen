# Экран "Диагностика" (scr_diagnostics)

## Файлы
- `main/ui/screens/scr_diagnostics.h` -- заголовок (API)
- `main/ui/screens/scr_diagnostics.c` -- реализация

## Назначение
Отображение системной диагностической информации контроллера ESP32-S3:
heap, uptime, состояние MQTT, статус Modbus-устройств, свободное место
в стеках задач RTOS.

## Компоновка экрана (1280 x 700 px)

```
+-------------------+------------------------------------------+
| System            | Modbus                                    |
| Heap free: 85432  | Device       | Status  | Errors          |
| Heap min:  42100  | Waveshare AI | Online  | 0               |
| Uptime: 02:14:35  | URZh2KM      | Online  | 2               |
| MQTT: Online      | SL21-201     | Offline | 15              |
|                   | SL21-101     | Online  | 0               |
| 380x200  y=16     | 840x200  x=420  y=16                    |
+-------------------+------------------------------------------+
| Task Stack Watermarks                           1240x240     |
| Task         | Free (bytes)                     y=236        |
| Modbus       | 1024 bytes                                    |
| IO           | 768 bytes                                     |
| Process      | 512 bytes                                     |
| Watchdog     | 2048 bytes                                    |
| MQTT         | 896 bytes                                     |
+-------------------------------------------------------------+
```

## Иерархия виджетов

```
cont (1280x700, COLOR_BG_DARK)
  +-- sys_panel (380x200, x=20, y=16)
  |     +-- title "System"
  |     +-- kv "Heap free" -> lbl_heap_free
  |     +-- kv "Heap min" -> lbl_heap_min
  |     +-- kv "Uptime" -> lbl_uptime
  |     +-- kv "MQTT" -> lbl_mqtt
  +-- mb_panel (840x200, x=420, y=16)
  |     +-- title "Modbus"
  |     +-- hdr: "Device" | "Status" | "Errors"
  |     +-- row[0..3]: name | lbl_mb_online[i] | lbl_mb_errors[i]
  +-- stk_panel (1240x240, x=20, y=236)
        +-- title "Task Stack Watermarks"
        +-- hdr: "Task" | "Free (bytes)"
        +-- row[0..4]: name | lbl_stack[i]
```

## Функции

| Функция | Описание |
|---|---|
| `scr_diagnostics_create(parent)` | Создаёт три панели с таблицами |
| `scr_diagnostics_update(cont, data)` | Обновляет все диагностические значения |
| `make_info_panel(...)` | Создаёт панель с заголовком (скруглённый прямоугольник) |
| `make_kv_row(...)` | Строка "ключ: значение" внутри панели |

## Источники данных (plant_data_t)

| Поле | Виджет | Формат |
|---|---|---|
| `diagnostics.heap_free` | lbl_heap_free | "%u bytes" |
| `diagnostics.heap_min` | lbl_heap_min | "%u bytes" |
| `diagnostics.uptime_s` | lbl_uptime | ui_fmt_uptime -> "dd:hh:mm:ss" |
| `mqtt_connected` | lbl_mqtt | "Online" / "Offline" |
| `diagnostics.modbus_online[0..3]` | lbl_mb_online[0..3] | "Online" / "Offline" |
| `diagnostics.modbus_errors[0..3]` | lbl_mb_errors[0..3] | "%u" |
| `diagnostics.stack_modbus` | lbl_stack[0] | "%u bytes" |
| `diagnostics.stack_io` | lbl_stack[1] | "%u bytes" |
| `diagnostics.stack_process` | lbl_stack[2] | "%u bytes" |
| `diagnostics.stack_watchdog` | lbl_stack[3] | "%u bytes" |
| `diagnostics.stack_mqtt` | lbl_stack[4] | "%u bytes" |

## Modbus-устройства

| Индекс | Имя | Назначение |
|---|---|---|
| 0 | Waveshare AI | Модуль аналоговых входов 4-20 мА (давление, температура) |
| 1 | URZh2KM | Расходомеры (Q1..Q4) |
| 2 | SL21-201 | Кондуктометр (S1, S2) |
| 3 | SL21-101 | Кондуктометр (S3) |

## Цветовая индикация

| Условие | Цвет |
|---|---|
| `heap_free < 32768` (32 KB) | COLOR_PUMP_FAULT (красный) |
| `heap_min < 16384` (16 KB) | COLOR_PUMP_FAULT (красный) |
| `stack < 512` | COLOR_PUMP_FAULT (красный) |
| `modbus_errors > 0` | COLOR_ALARM_WARNING (жёлтый) |
| `modbus_online = false` | COLOR_PUMP_FAULT (красный) |
| `mqtt_connected = false` | COLOR_PUMP_FAULT (красный) |

## Взаимодействие пользователя
Экран только для чтения. Нет кнопок и интерактивных элементов.

## Механизм обновления
- Периодический (таймер ~500 мс)
- `scr_diagnostics_update()` обновляет текст и цвет каждой метки

## Зависимости
- `ui_theme.h` -- цвета
- `ui_common.h` -- `ui_fmt_uptime()`
- `ui_fonts.h` -- шрифты
- `lang.h` -- строки (STR_DIAG_*, STR_STATUS_*)
- `plant_data.h` -- diagnostics_t
