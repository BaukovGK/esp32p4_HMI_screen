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
- [x] `ui_modal.h/.c` — generic overlay: backdrop (60% черный, click-close)
      + card (560×640 max, rounded, shadow) + header (title/subtitle/×) +
      body (flex column scrollable) + footer (Закрыть). Helper'ы:
      ui_modal_add_section / add_prop_row / add_current_value (big value
      + state badge) / add_range_bar (10-pct позиция).
- [x] `ui_sensor_modal_show(tag, value)` — порт showSensorDetail.
      sensor_meta_t static таблица (10 датчиков: P1..P4, Q1..Q4, σ2, σ3)
      с порогами warn/alarm, единицами, modbus-source. Tolerant lookup:
      UTF-8 "σ2" → fallback "S2" в коде ищется как tag_alt.
      Секции: ТЕКУЩЕЕ ЗНАЧЕНИЕ (value+badge+range bar), УСТАВКИ
      (норма/warn/alarm), ИСТОЧНИК (Modbus/Обновлено).
- [x] ui_sensor click callback: ui_sensor_set_click_cb(sensor, cb).
      Внутри LVGL EVENT_CLICKED hook'ается; cb получает (tag, value).
      В scr_mnemonic зарегистрирован общий on_sensor_click для всех 10.
- [x] `ui_equipment_modal_show(equipment_id)` — порт showEquipmentDetail.
      id'ы: filter / pump-pre / pump-st1 / pump-st2 / membrane-1 / membrane-2.
      Dispatcher по id находит соответствующий filter_meta_t / pump_meta_t /
      membrane_meta_t из static таблиц и вызывает render_*_modal().
      Filter: ΔP + cartridge runtime+progress bar.
      Pump: state badge + run hours (continuous/total/starts) + ratings.
      Membrane: rejection + wash time progress + lifetime progress.
      Helper ui_modal_add_progress_bar (filled bar 0..pct% с state-цветом).
      Клик-handler в scr_mnemonic: attach_equipment_click(obj, id_string).
      Все 6 equipment'ов подключены: filter + 3 pumps + 2 membranes.

### Фаза 3 итого ✅ — все модалы готовы.

### Фаза 4 — Промывка ✅
- [x] Полностью переписан `scr_washing.c` под прототип proto/washing.html:
      Left card (≈864 px) — title "Режим промывки" + badge "Мойка" +
      compact 4-phase track (Ожидание/Нагрев/Подача/Слив, по 36 px) +
      info-strip (Прошло/Осталось/Текущая/Целевая в одной полосе 64 px,
      разделены divider'ом) + wash-mnemo placeholder (под будущую
      CIP-схему).
      Right column (380 px) — 3 stacked cards:
        Состояние оборудования (5 строк: Преднагн/Нагреватель/насосы/Дозатор)
        Действия (4 кнопки: Начать нагрев/Подтв. подачу/Подтв. слив/Отменить)
        Подсказка (текст с инструкцией оператору).
- [x] Map 7 wash_sub_state_t → 4 видимых фазы (map_wash_state helper).
      Каждая фаза имеет 3 визуальных состояния: pending (bg-mute) /
      active (accent-mute + accent border) / done (accent-mute + success
      border).
- [x] Кнопки enable'ятся в зависимости от текущего sub-state:
      btn_heat   ↔ WAIT_HEAT
      btn_supply ↔ WAIT_SUPPLY
      btn_drain  ↔ WAIT_DRAIN
      btn_cancel — always enabled
- [x] Equipment status badges обновляются из data->di/do_bits/doser.
- [ ] (TODO в фазе 6) Time elapsed/remaining — пока mock "--:--",
      реальные значения придут по washing/elapsed_s MQTT-topic'у.

### Фаза 5 — Настройки + Отладка ✅ (минимальная миграция)
- [x] COLOR_* макросы в ui_theme.h перенаправлены на ui_token_*()
      функции. Старые экраны (scr_settings, scr_diagnostics, scr_alarms,
      scr_parameters, ui_common::alarm_bar/nav_bar) теперь автоматически
      адаптируются под активную light/dark тему. Visual consistency
      достигнута без полного rewrite'а 913 строк.
- [ ] (Опционально, низкий приоритет) Полная перерисовка scr_settings/
      diagnostics под layout из proto/settings.html и proto/debug.html
      (220px nav + content, log + 320px sidebar). Текущие версии
      функционально полноценны, отличаются только versткой.

### Фаза 6 — runtime интеграция
- [ ] Кнопки `[data-action]` → `ui_events.c` подписки на MQTT-команды.
- [ ] Live-обновление значений из `plant_data_t` (уже есть refresh-таймер
      250 ms в `ui_main.c`).
- [ ] Локализация: `data-i18n` → ключи в `i18n/lang_strings.h`.
- [ ] Persistence: `localStorage.theme/lang` → NVS (`nvs_worker.c`).

### Фаза 7 — анимации
- [x] Pipe-flow: marching dashes overlay (white 70% opacity) поверх
      active pipes. Реализовано через extended-line + translate_x/y по
      направлению потока, clipping контейнером на bbox трубы.
- [x] Recycle-march: animation translate на dashed pipe-recycle лини
      (период 16 px, 1.6 s).
- [x] Tank-wave: surface highlight (2px белая) translate_x -10..+10 за
      5s с ease-in-out playback — "дышащая" поверхность воды.
- [x] Pump-spin: непрерывное вращение ротора (transform_rotation) с
      разной скоростью для running (1.5s) и starting (3s).
- [x] Pump-pulse: opacity 100..50% для starting (1.2s) и error (1.0s)
      состояний.
- [x] Sensor-pulse (DANGER): opacity 100..40% на frame, 1.5s полный цикл
      с ease-in-out playback. WARN остаётся статичным.
- [ ] (TODO low) Учёт prefers-reduced-motion через config-флаг
      (см. UI_SPEC §11). Сейчас все анимации всегда включены.

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
