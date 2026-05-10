# Native LVGL simulator (macOS / Linux)

Запуск UI HMI без ESP32 — окно SDL2 с полным интерфейсом, все экраны
кликабельны, mock-данные обновляются каждые 250 мс. Полезно для:
- Быстрая итерация по дизайну (экономия 30+ сек на каждый flash)
- Отладка анимаций и LVGL-багов под `lldb`
- Скриншоты экранов для документации
- Демо без подключения железа

## Установка зависимостей

**macOS (Homebrew):**
```bash
brew install sdl2 cmake
```

**Linux (Debian/Ubuntu):**
```bash
sudo apt install libsdl2-dev cmake build-essential pkg-config
```

## Сборка и запуск

Из корня проекта:
```bash
./sim/build.sh           # configure + build + run
./sim/build.sh build     # только собрать (бинарь в sim/build/ro_hmi_sim)
./sim/build.sh clean     # удалить build-директорию
```

Или напрямую через CMake:
```bash
cmake -S sim -B sim/build -DCMAKE_BUILD_TYPE=Debug
cmake --build sim/build -j
./sim/build/ro_hmi_sim
```

Esc или ⌘Q закрывают окно.

## VS Code

В проекте уже сконфигурированы tasks/launch (`.vscode/`):
- **⇧⌘B → "sim: build"** — собрать.
- **F5 → "sim: launch"** — запустить под отладчиком (lldb).
- **⌘⇧P → Run Task → "sim: run"** — собрать и запустить без debugger'а.

## Архитектура

Симулятор переиспользует тот же UI-код, что собирается под ESP32 —
включая LVGL из `managed_components/lvgl__lvgl/` (без version drift).

```
sim/
├── CMakeLists.txt    # собирает LVGL + main/ui/* + main/i18n/* + main/fonts/*
├── lv_conf.h         # LVGL config: SDL backend, 32-битный цвет, full widgets
├── sim_main.c        # SDL window + lv_init + event loop
├── stubs/
│   └── esp_log.h     # подмена ESP_LOG* на printf-варианты
├── build.sh          # convenience-обёртка над CMake
└── README.md
```

В сборке симулятора определён `-DLVGL_LIVE_PREVIEW`:
- `main/ui/ui_main.c` — пустой (выключается через `#ifndef`)
- `main/ui/lvgl_preview.c` — активный entry point с mock-данными
  (mock plant_data, alarm_ring, mqtt stubs)
- `main/board/board.h` — header-only stubs для GPIO_NUM_*

Бинарь линкуется с `libSDL2` через `sdl2-config` (или pkg-config если
установлен). LVGL компилируется со встроенным SDL backend
(`src/drivers/sdl/lv_sdl_*.c`).

## Известные ограничения

- **Глифы.** Кастомные `Montserrat_*_4.c` шрифты в проекте созданы под
  ASCII + cyrillic + цифры. Не содержат: `☾` (U+263E theme btn),
  `·` (U+B7 middle dot), `σ` (U+3C3 Greek sigma), LV_SYMBOL_*
  (FontAwesome U+F0xx). При запуске будут warnings вида
  `glyph dsc. not found`. Текст всё равно рендерится без этих символов.
  Чинится: пересоздать шрифты с расширенным glyph-range через
  `lv_font_conv` (см. `main/fonts/doc_ui_fonts.md`).
- **Theme switch.** Кнопка ☾ в statusbar пока не пересоздаёт UI после
  смены темы — TODO в `lvgl_preview.c::on_theme_change`.
- **Touch vs mouse.** Симулятор использует мышь. На тачскрине жесты
  (gestures) могут отличаться. Базовая логика касаний работает.
- **Производительность.** На macOS симулятор работает ~30-60 fps
  на 1280×800 (зависит от анимаций). На реальном ESP32-P4 — другие
  ограничения (PSRAM, GPU). Профилировать только по симулятору
  нельзя.

## Полезные приёмы

**Debug отдельных виджетов** — модифицируйте `lvgl_live_preview_init()`
чтобы вместо мнемосхемы создавать только тестируемый виджет:
```c
ui_pump_config_t cfg = { .cx = 200, .cy = 200,
                          .label = "Test", .state = UI_PUMP_RUNNING };
ui_pump_create(s_content, &cfg);
```

**Скриншот** — на macOS ⇧⌘4 → клик по окну. PNG автоматически
сохранится на Рабочий стол.

**Live reload** — пока нет, но можно настроить через `entr`:
```bash
find main -name '*.c' -o -name '*.h' | entr -r ./sim/build.sh
```
