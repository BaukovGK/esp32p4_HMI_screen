# LVGL implementation roadmap

Перенос HTML-прототипа (`proto/`) на LVGL 9.2 / esp_lvgl_port для
ESP32-P4-NANO. Ветка работ: `feature/lvgl-ui`.

> Источники истины:
> - **Дизайн-токены:** `docs/UI_TOKENS.md` ↔ `proto/style.css` ↔ `main/ui/ui_tokens.h/.c`
> - **Экраны:** `docs/UI_SCREENS.md` ↔ `proto/*.html`
> - **Спецификация:** `docs/UI_SPEC.md` (поведение, ISA-5.1, модалы, equipmentMeta)

## Принципы

1. **Не ломаем сборку.** Каждый коммит компилируется и устанавливается на стенд.
2. **Постепенная миграция.** Старые экраны (`scr_*.c` с COLOR_BG_DARK и т.д.)
   живут параллельно с новыми токенами. Переписываем по одному.
3. **Прототип — первоисточник.** Любое изменение визуала сначала уходит
   в `proto/*`, потом портируется в LVGL — иначе проект расходится.
4. **Хост-тесты для логики.** Виджеты теста не имеют, но
   data-классификация (state по порогам, formatHours, ΔP=P1−P2) — да.

## Чек-лист реализации

### Фаза 0 — фундамент ✅
- [x] `ui_tokens.h/.c` — light/dark палитры, accessor-функции
- [x] Скаффолд `feature/lvgl-ui` ветки
- [ ] (этот документ)

### Фаза 1 — каркас навигации ✅
- [x] `ui_statusbar.h/.c` — 44px полоса: время · сообщение · соединение ·
      кнопки theme/lang. Заменяет старый `alarm_bar` (40px).
- [x] `ui_tabbar.h/.c` — 64px нижняя панель с 4 вкладками: Главная /
      Промывка / Настройки / Отладка (LV_SYMBOL_HOME/REFRESH/SETTINGS/LIST
      + локализованные лейблы). MVP = 4 экрана (см. UI_SPEC §1).
- [x] **Migration в `ui_main.c`:** ui_alarm_bar_* → ui_statusbar_*,
      ui_nav_bar_* → ui_tabbar_*. TAB_TO_SCREEN маппинг (4 tab → 4 screen).
      ui_theme.h legacy-макросы UI_SCREEN_WIDTH/UI_CONTENT_HEIGHT/etc.
      превращены в алиасы на UI_*_W/H из ui_tokens.h — старые экраны
      автоматически адаптируются к новому layout (700→692, 40→44, 60→64).
- [ ] Удалить устаревшие `ui_alarm_bar_*` и `ui_nav_bar_*` из ui_common.c
      (после того, как все экраны перейдут на новые токены).

### Фаза 2 — мнемосхема (главный экран) ✅
Контейнер: `lv_canvas` 900×530 (после crop viewBox 0 30 900 530), либо
композитные `lv_obj`-виджеты (предпочтительно — анимации проще).

Компоненты (см. `proto/index.html` + `docs/UI_SPEC.md` §9):
- [x] **Tank widget** (`ui_tank.h/.c`) — shell rect (clip_corner) +
      water rect с linear gradient (light cyan → deep blue, LV_GRAD_DIR_VER)
      + surface highlight 2px + name + pct (динамически центрируется на воде) +
      probe rod (vertical 2px line) + N×level switches (8×8 dots + tags).
      API: `ui_tank_create()` / `ui_tank_set_fill()` / `ui_tank_set_switch_state()`.
      Контекст в user_data shell'а, авто-освобождение через LV_EVENT_DELETE.
      ⚠️ TODO: анимация волны (translateX, 5s ease-in-out) — фаза 7.
      ⚠️ TODO: dashed pattern на probe — пока solid с opacity 55%.
- [x] **Pump widget** (`ui_pump.h/.c`) — circle r=26 с rotating impeller
      (cross-pattern из 2 перпендикулярных лопастей + hub). 4 состояния
      через `ui_pump_set_state()`: running (1.5s rotation), starting
      (3s rotation + 1.2s opacity pulse), error (no rotation +
      1.0s pulse), off (static). LVGL anim на transform_rotation
      (0..3600 = 0..360°).
- [x] **Filter widget** (`ui_filter.h/.c`) — rect 28×28 повёрнутый на 45°
      даёт ромб 40×40 (диагональ). Буква "Ф" поверх (UI_FONT_SM).
      Подзаголовок "Фильтр" под ромбом.
- [x] **Membrane widget** (`ui_membrane.h/.c`) — rect с label сверху
      и dashed-divider посередине (имитация раздела элементов 4040).
- [x] **Sensor circle** (`ui_sensor.h/.c`) — leader (lv_line) + tap-point
      + circle root c frame/divider/tag/value. 4 состояния:
      ok/warn/danger/offline. Все 3 элемента (leader, tap, circle) —
      siblings; контекст хранит ссылки и удаляет их вместе.
- [x] **Pipe helpers** (`ui_pipe.h/.c`) — `ui_pipe_segment(p, x1,y1,x2,y2,
      kind)` для горизонтальных/вертикальных сегментов через lv_line;
      kind = INACTIVE/ACTIVE/RECYCLE. `ui_pipe_junction(p, cx, cy)` —
      filled circle 8×8 для узла трубопровода.
- [x] **Mnemo composer** (`scr_mnemonic.c` rewrite) — собирает всю
      мнемосхему по координатам из `proto/index.html` через SX(svg)/
      SY(svg-30) helpers (учёт viewBox 0 30 900 530 → MNEMO_PX).
      Все 3 ёмкости + 3 насоса + фильтр + 2 мембраны + дозатор +
      10 датчиков + ~20 трубопроводов + 3 junction'а размещаются на
      MNEMO_PX_W × MNEMO_PX_H канвасе с горизонтальным letterbox'ом.
      `scr_mnemonic_update()` обрабатывает dirty-flags ANALOG/FLOW/
      CONDUCTIVITY/IO и обновляет sensor values + tank switches +
      pump states по data->pressure/flow/conductivity/di.

### Фаза 3 — модалы
- [ ] `ui_modal.h/.c` — generic overlay с close-button, backdrop click.
- [ ] `ui_sensor_modal_show(tag)` — порт showSensorDetail (range bar,
      trend, setpoints, source). `sensorMeta` → C-структура.
- [ ] `ui_equipment_modal_show(equipment_id)` — порт showEquipmentDetail
      (filter ΔP+runtime / pump state+hours / membrane wash+lifetime).
      `equipmentMeta` → C-структура.

### Фаза 4 — Промывка
- [ ] Заменить `scr_washing.c` на новый layout: compact phase-track +
      info-strip (timers + temps в одной полоске) + wash-mnemo placeholder.
- [ ] Поведение фаз: idle → heat → circ → drain (см. UI_SPEC §5).

### Фаза 5 — Настройки + Отладка
- [ ] Перерисовать `scr_settings.c` под новую вёрстку (220px nav-list +
      content).
- [ ] Перерисовать `scr_diagnostics.c` под новую вёрстку (debug log +
      sidebar 320px).

### Фаза 6 — runtime интеграция
- [ ] Кнопки `[data-action]` → `ui_events.c` подписки на MQTT-команды.
- [ ] Live-обновление значений из `plant_data_t` (уже есть refresh-таймер
      250 ms в `ui_main.c`).
- [ ] Локализация: `data-i18n` → ключи в `i18n/lang_strings.h`.
- [ ] Persistence: `localStorage.theme/lang` → NVS (`nvs_worker.c`).

### Фаза 7 — анимации (low priority)
- [ ] Pipe-flow: бегущие капли поверх активной трубы (анимация по trail).
- [ ] Recycle-march: anim на pipe-recycle dashed.
- [ ] Tank-wave: oscillating translate.
- [ ] Pump-spin: непрерывное вращение ротора.
- [ ] Sensor-pulse (.danger): periodic stroke-width.
- [ ] Учёт `prefers-reduced-motion` (через config-флаг, см. UI_SPEC §11).

## Architecture decisions

### Координаты
Прототип использует SVG viewBox `0 30 900 530`. На железе с экраном
1280×800 контентная зона = 1280×692. Соотношения:
```
scale_x = 1280 / 900 = 1.422
scale_y = 692  / 530 = 1.306
```
Поскольку aspect-ratio'ы расходятся (1.30 SVG vs 1.85 экран), копировать
координаты «как есть» нельзя. Варианты:
1. **Margin-letterbox** — мнемосхема 900×530 пикселей в физических, по
   центру, с отступами по бокам (180+180 px). Просто, но теряем место.
2. **Scale-to-fit** с сохранением пропорций (scale = min(1.422, 1.306) =
   1.306). Мнемосхема становится 1175×692. Левый/правый margin по 52 px.
3. **Адаптивный layout** — оставить SVG-структуру как набор позиций в
   relative units, пересчитывать при создании виджета.

→ **MVP**: вариант 2 (scale-to-fit). Координаты внутри мнемо умножаются на
1.306 при создании виджетов.

### Анимации
LVGL не поддерживает stroke-dashoffset. Альтернативы:
- **Flow-march**: вертикально/горизонтально движущийся 8-px капель-pattern
  через `lv_anim_t` на `x` или `y` маски. Каждая активная труба =
  отдельный объект с маской.
- **Tank-wave**: периодически смещаемый sine-pattern через canvas
  `lv_canvas_draw_*` или статичный фон с анимированным offset.
- **Pump-spin**: `lv_image_set_rotation()` на спрайте импеллера —
  встроенная LVGL-анимация работает.
- **Pulse (danger)**: `lv_anim_t` на opacity или border-width.

Бюджет производительности: ESP32-P4 с PSRAM держит 60 fps на 1280×800
с LVGL Vector Graphics, но при ~20 одновременных анимациях fps может
просесть. План: профилировать после фазы 2.

### Память
- LVGL buffer: 2 × 1280 × 100 × 2 bytes (RGB565) = 512 KB → в PSRAM.
- Экранные объекты: ~5 KB / экран × 4 экрана = 20 KB.
- Trend ring buffers: 60 × 4 bytes × 10 sensors = 2.4 KB.
- Modal overlays: создаются on-demand, удаляются по close.

### Шрифты
Уже есть Montserrat 12/14/16/18/20/22/24/28/36 (см. `main/fonts/`).
Нет 48px (для аварий-fs-3xl) — TODO добавить генерацию или заменить
на двойной 24px при необходимости.

## Что портируем из прототипа в первую очередь

Приоритет = частота/важность для оператора:
1. **Главная (мнемосхема)** — оператор смотрит её 90% времени.
2. **Промывка** — критичный регулярный workflow.
3. **Настройки** — редко, но без неё нельзя ввести в эксплуатацию.
4. **Отладка** — для запуска и редких troubleshoot-сессий.

Внутри Главной приоритет:
1. KPI справа + кнопки управления (на видном месте, простая разметка).
2. Tank-widget'ы (3 шт) — базовый компонент.
3. Pump+Filter+Membrane виджеты.
4. Sensor circles + tap-points + leader-lines.
5. Pipes (active + recycle).
6. Анимации (опционально на первом этапе).
7. Click-handlers + модалы (после того как все виджеты на месте).

## Переписываем ли существующие экраны?

Существующие `scr_mnemonic.c`, `scr_washing.c`, `scr_settings.c`,
`scr_diagnostics.c`, `scr_parameters.c`, `scr_alarms.c` — содержат
логику работы с данными (`plant_data_t`, MQTT). При переписывании:
- **Сохраняем** модель данных, refresh-таймер, MQTT-подписки.
- **Меняем** внешний вид: токены, виджеты, layout.
- **Сворачиваем** parameters/alarms в Главную (KPI справа) либо
  оставляем, но в MVP они не tab-bar items (см. UI_SPEC §1).

В рамках MVP в tabbar 4 экрана: Главная (= scr_mnemonic),
Промывка (= scr_washing), Настройки (= scr_settings), Отладка
(= scr_diagnostics). `scr_parameters` и `scr_alarms` уходят в архив или
становятся под-страницами.
