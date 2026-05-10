# UI Design Tokens

> Дизайн-токены HMI. Палитра ориентирована на корпоративный стиль НТТ
> (`proto/цвета.png` — тёмно-зелёный + белый/серый), dark theme в стиле
> современных IDE (`proto/темная тема.png` — мягкий тёмно-серый, не чёрный).
>
> Готовы к портированию в LVGL через `lv_style_t` или global theme.

---

## 1. Цветовая палитра

### 1.1 Light theme

Основной фон светлый, акценты — корпоративный зелёный.

| Token | Hex | Использование |
|---|---|---|
| `--bg-base` | `#f5f7fa` | Базовый фон экрана (за карточками) |
| `--bg-card` | `#ffffff` | Фон карточек, status/tab bar |
| `--bg-elev` | `#ffffff` | Modal, элевейтед поверхности |
| `--bg-mute` | `#eef0f3` | Disabled, второстепенные кнопки, разделители |
| `--text-primary` | `#1a1d23` | Основной текст, KPI значения |
| `--text-secondary` | `#5a6068` | Описания, легенды |
| `--text-muted` | `#8a9099` | Подписи, неактивный текст |
| `--text-inverse` | `#ffffff` | Текст на цветной кнопке |
| `--border` | `#d8dde2` | Рамки карточек |
| `--border-strong` | `#b8bfc7` | Активные/выделенные рамки |
| `--accent` | `#2d8659` | **Корпоративный зелёный** (primary actions) |
| `--accent-hover` | `#1f6b45` | Press-state |
| `--accent-mute` | `#dcefe4` | Background-tint (active tab, hover) |
| `--success` | `#16a34a` | OK, AUTO RUNNING, online |
| `--success-bg` | `#dcfce7` | Success-badge, KPI в норме |
| `--warning` | `#ca8a04` | WARN-алармы, промывка идёт |
| `--warning-bg` | `#fef3c7` | Warning-badge |
| `--danger` | `#dc2626` | ALARM/CRITICAL, аварийный стоп |
| `--danger-bg` | `#fee2e2` | Danger-badge, pulse-bg |
| `--info` | `#0891b2` | INFO-сообщения, синий акцент |
| `--info-bg` | `#cffafe` | Info-badge |

### 1.2 Dark theme

Стиль современной IDE (RustRover, VS Code) — мягкий тёмно-серый, не чёрный, акценты остаются зелёными но чуть ярче.

| Token | Hex | Использование |
|---|---|---|
| `--bg-base` | `#1e2128` | Базовый фон экрана |
| `--bg-card` | `#252932` | Фон карточек |
| `--bg-elev` | `#2c313c` | Modal, элевейтед |
| `--bg-mute` | `#1a1d23` | Disabled, разделители |
| `--text-primary` | `#e6e9ed` | Основной текст |
| `--text-secondary` | `#a8aeb6` | Описания |
| `--text-muted` | `#6c727b` | Подписи, неактивный |
| `--text-inverse` | `#1e2128` | Текст на ярких кнопках |
| `--border` | `#363b47` | Рамки |
| `--border-strong` | `#4a5161` | Активные рамки |
| `--accent` | `#3fa66a` | Корпоративный зелёный (немного ярче для контраста) |
| `--accent-hover` | `#52c179` | Hover/active |
| `--accent-mute` | `#1a3826` | Background-tint |
| `--success` | `#22c55e` | OK |
| `--success-bg` | `#14532d` | Success-bg |
| `--warning` | `#eab308` | WARN |
| `--warning-bg` | `#422006` | Warning-bg |
| `--danger` | `#ef4444` | CRITICAL |
| `--danger-bg` | `#450a0a` | Danger-bg |
| `--info` | `#06b6d4` | INFO |
| `--info-bg` | `#164e63` | Info-bg |

### 1.3 Семантика

| Категория | Light | Dark |
|---|---|---|
| **Primary action** (Пуск AUTO, Сохранить) | accent (#2d8659) | accent (#3fa66a) |
| **Destructive** (Стоп, Отменить мойку, E-STOP) | danger (#dc2626) | danger (#ef4444) |
| **Secondary action** (Отмена в modal) | bg-mute + border | bg-mute + border |
| **Success state** (всё в норме) | success (#16a34a) | success (#22c55e) |
| **Warning state** (засорение фильтра, near-limit) | warning (#ca8a04) | warning (#eab308) |
| **Critical state** (E-STOP active, OVERHEAT) | danger pulse-anim | danger pulse-anim |
| **Inactive trubs** (мнемосхема) | grey (#94a3b8) | grey (#64748b) |
| **Active flow** (мнемосхема) | accent | accent |
| **Recycle/concentrate** (мнемосхема) | info dashed | info dashed |

---

## 2. Типографика

### 2.1 Шрифт

**Системный sans-serif.** На ESP32-P4 LVGL используем встроенный Montserrat:
- Основной: Montserrat 14, 16, 20, 28
- Tabular numerics: Montserrat 16, 20, 28 (для KPI и времени)

### 2.2 Размеры

| Token | px | Использование |
|---|---:|---|
| `--fs-xs` | 12 | Подписи, единицы измерения, sensor-tags на схеме |
| `--fs-sm` | 14 | Описания, legend, secondary buttons |
| `--fs-md` | 16 | Body text, primary buttons |
| `--fs-lg` | 20 | Заголовки секций, KPI значения |
| `--fs-xl` | 28 | Заголовки экранов, primary KPI на главной |
| `--fs-2xl` | 36 | Hero значения (температура промывки) |
| `--fs-3xl` | 48 | Алярм-banner (если будет на главном) |
| `--fs-display` | 64 | Текущая температура промывки, таймеры |

### 2.3 Веса

- 400 — body
- 500 — labels, secondary buttons
- 600 — section titles, KPI labels
- 700 — KPI values, alarm titles, badges

### 2.4 Spacing scale

| Token | px | Применение |
|---|---:|---|
| `--gap-xs` | 4 | Внутри chip/badge |
| `--gap-sm` | 8 | Между связанными элементами |
| `--gap-md` | 12 | Между группами в карточке |
| `--gap-lg` | 16 | Между карточками |
| `--gap-xl` | 24 | Между секциями экрана |
| `--gap-2xl` | 32 | Padding в больших modal |

---

## 3. Геометрия (touch targets)

| Элемент | Min size | Recommended |
|---|---:|---:|
| **Кнопка primary** | 60×60 px | 60×120 px |
| **Кнопка large** (важные команды: Пуск AUTO, Стоп) | 80×160 px | 80×200 px |
| **Tab bar item** | 64×183 px | 64×183 px (1280/7) |
| **Status bar button** | 32×48 px | 32×48 px |
| **Number input ± кнопка** | 56×56 px | 56×56 px |
| **List item** (алармы, история) | 56 px высоты | 64 px высоты |
| **Modal width** | 480 px | 600 px |
| **Toast width** | 320 px | 400 px |

Между интерактивными элементами — **min 8 px gap** (избегаем ложных касаний).

---

## 4. Радиусы скругления

| Token | px | Где |
|---|---:|---|
| `--radius-sm` | 4 | Маленькие элементы (badge, chip) |
| `--radius-md` | 8 | Кнопки, input, tab |
| `--radius-lg` | 12 | Карточки, modal |
| `--radius-pill` | 999 | Pill-badge (status, mode) |

---

## 5. Тени

В embedded LVGL тени дороги (re-rendering). Используем минимально:

| Token | Where | Value |
|---|---|---|
| `--shadow-card` | карточки | НЕ используем, граница вместо тени |
| `--shadow-modal` | confirm modal | `0 8px 32px rgba(0,0,0,0.2)` (только в light) |
| `--shadow-toast` | алярм-banner | `0 4px 16px rgba(0,0,0,0.15)` |

В dark — тени не используются вообще (работаем поверх elev-контраста).

---

## 6. Анимации

| Анимация | Где | Длительность |
|---|---|---:|
| **fade-in toast** | новый аларм | 200 мс |
| **pulse critical** | KPI вне диапазона, активный CRITICAL аларм | 1500 мс цикл |
| **slide modal** | confirm dialog | 200 мс |
| **tab switch** | переключение вкладок | 150 мс crossfade |
| **screen change** | переход между экранами | НЕ используем (мгновенно) |
| **flow-march** | имитация движения воды по активным трубам (sw-pipe-flow overlay) | 1400 мс цикл |
| **recycle-march** | то же на dashed-линиях рециклов | 1600 мс цикл |

### 6.1 Flow animation (движение воды по трубам)

В прототипе (`proto/index.html`) реализовано через два слоя:
1. **Базовая линия** `pipe-active` — сплошная толстая (5 px) accent-цвета
2. **Overlay** `pipe-flow` — поверх, dashed (`stroke-dasharray: 6 10`),
   полупрозрачный белый (`rgba(255,255,255,0.7)`), анимация
   `stroke-dashoffset` с -16 за 1400 мс линейно

Создаёт иллюзию «бегущих капель» воды. На рециклах (`pipe-recycle.flowing`)
анимируется напрямую stroke-dashoffset существующей dashed-линии.

В LVGL 9.2 нет встроенного `stroke-dashoffset`, но эффект реализуется
через **спрайтовую анимацию** на `lv_obj_t` с маской:
```c
// Создать lv_obj покрывающий активный сегмент трубы.
// Применить mask с чередующимися прозрачными и непрозрачными
// полосами, сдвигать mask раз в N мс через lv_anim_set_exec_cb.
// На P4 даже с PSRAM это будет fillrate-bound — рекомендую 4-8 fps.
```

Альтернативно — отрисовка через `lv_canvas` с обновлением буфера каждый
тик. Это дороже но проще программно.

⚠️ Для оптимизации ESP32-P4 с PSRAM — анимация **только на видимых**
сегментах труб, и **только когда state = AUTO/RUNNING или WASHING**
(в IDLE и FAULT — статичные линии без анимации).

### 6.2 Reduced motion (a11y)

Все анимации (`pulse`, `flow-march`, etc) должны **отключаться** при
`prefers-reduced-motion: reduce`. В LVGL — глобальный флаг
`s_reduce_motion` в settings + проверка в `lv_anim_*` calls.

⚠️ Все анимации **disabled** по global flag в settings → "Reduced motion" (для оптимизации производительности при слабой PSRAM/CPU).

---

## 7. Иконография

Используем **LVGL built-in symbols** (LV_SYMBOL_*) или Unicode-emoji.

| Назначение | Символ |
|---|---|
| Главная (мнемосхема) | ⌂ (`LV_SYMBOL_HOME`) |
| Промывка | ⟲ или 🧪 |
| Параметры | ☰ или 📊 |
| Алармы | ⚠ (`LV_SYMBOL_WARNING`) |
| Энергия | ⚡ или ⚙ |
| Диагностика | ⚒ или 🔧 |
| Настройки | ⚙ (`LV_SYMBOL_SETTINGS`) |
| Журнал | 📋 |
| Тема светлая | ☀ |
| Тема тёмная | ☾ |
| Связь онлайн | ● зелёная точка |
| Связь оффлайн | ✕ красный крест |
| PIN заперт | 🔒 |
| PIN открыт | 🔓 |
| Пуск/Старт | ▶ |
| Стоп | ⏹ |
| Запускается | ↻ (rotation animation) |
| Ошибка | ✕ |

---

## 8. Mapping в LVGL

### 8.1 Создание themes

```c
// ui/ui_theme.c
#include "lvgl.h"

typedef enum { THEME_LIGHT, THEME_DARK } theme_id_t;

typedef struct {
    lv_color_t bg_base, bg_card, bg_elev, bg_mute;
    lv_color_t text_primary, text_secondary, text_muted, text_inverse;
    lv_color_t border, border_strong;
    lv_color_t accent, accent_hover, accent_mute;
    lv_color_t success, warning, danger, info;
} ui_palette_t;

static const ui_palette_t s_palette_light = {
    .bg_base       = LV_COLOR_MAKE(0xf5, 0xf7, 0xfa),
    .bg_card       = LV_COLOR_MAKE(0xff, 0xff, 0xff),
    // ... остальное
    .accent        = LV_COLOR_MAKE(0x2d, 0x86, 0x59),
};

static const ui_palette_t s_palette_dark = {
    .bg_base       = LV_COLOR_MAKE(0x1e, 0x21, 0x28),
    // ...
    .accent        = LV_COLOR_MAKE(0x3f, 0xa6, 0x6a),
};

void ui_theme_apply(theme_id_t id);
const ui_palette_t *ui_theme_palette(void);
```

### 8.2 ISA-5.1 sensor circle (приборные индикаторы на мнемосхеме)

Стандарт P&ID — круг с тегом сверху и значением снизу:

```c
// ui/ui_sensor_circle.c
typedef struct {
    lv_obj_t *container;   // прозрачный контейнер
    lv_obj_t *frame;       // lv_arc или lv_canvas с кругом
    lv_obj_t *divider;     // тонкая горизонтальная линия
    lv_obj_t *tag_label;   // "P1", "Q3", "σ2"
    lv_obj_t *value_label; // "3.2", "26.8", "1.2"
    int last_state;        // ok / warn / danger / offline
} sensor_circle_t;

sensor_circle_t *sensor_circle_create(lv_obj_t *parent, const char *tag);
void sensor_circle_set_value(sensor_circle_t *sc, float value, const char *fmt);
void sensor_circle_set_state(sensor_circle_t *sc, int state);
void sensor_circle_set_offline(sensor_circle_t *sc, bool offline);
```

Размеры:
- Радиус круга 22 px (44 px диаметр)
- font-size тег: 9 px
- font-size значение: 11 px
- font-size единицы (опц.): 7 px

При state=danger — `lv_anim_*` на stroke-width (2 → 3.5 → 2 cycle 1.5s).

### 8.3 Стили компонентов

```c
// ui/ui_styles.c
static lv_style_t style_card;
static lv_style_t style_btn_primary;
static lv_style_t style_btn_danger;
static lv_style_t style_kpi_value;
// ...

void ui_styles_init(void) {
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, ui_theme_palette()->bg_card);
    lv_style_set_border_color(&style_card, ui_theme_palette()->border);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_pad_all(&style_card, 16);
    // ...
}

// При смене темы — пересоздать все стили + lv_obj_invalidate(lv_scr_act())
```

### 8.3 Шрифты

```c
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_28);

#define FONT_BODY      &lv_font_montserrat_16
#define FONT_LABEL     &lv_font_montserrat_14
#define FONT_KPI       &lv_font_montserrat_20  // tabular-numerics
#define FONT_TITLE     &lv_font_montserrat_28
#define FONT_DISPLAY   &lv_font_montserrat_28  // 28 max в встроенных, для display нужно сгенерить 36/48/64
```

⚠️ **TODO:** для `--fs-display` (64px) нужно сгенерировать кастомный font через LVGL Online Font Converter (https://lvgl.io/tools/fontconverter). Только цифры + точка + минус (~13 символов) → ~5 KB.

---

## 9. Изменения в `proto/`

После согласования палитры обновить прототип:

1. `proto/style.css` — заменить `--accent` `#2563eb` (синий) на `#2d8659` (зелёный НТТ)
2. Обновить `--accent-mute` соответственно
3. Перепроверить все hover/active states в Dark theme

Это сделать одним коммитом ПОСЛЕ согласования финальной палитры с заказчиком (палитра в `цвета.png` приблизительная — точные hex могут отличаться).

---

## 10. Checklist для разработчика LVGL

При портировании:
- [ ] Создать `ui_theme.{c,h}` с двумя палитрами + setter
- [ ] Создать `ui_styles.{c,h}` с базовыми стилями (card, btn primary/danger/secondary, kpi, badge)
- [ ] При смене темы — пересоздать все стили + invalidate
- [ ] Сгенерировать font Montserrat 28/36/48/64 для display-цифр
- [ ] Все `lv_color_t` через `ui_theme_palette()->X`, никаких хардкодов в screens
- [ ] Touch-target минимум 60×60 (assert в debug-build)
- [ ] Tab bar high-priority (всегда видна, кроме splash и first-boot wizard)
- [ ] Pulse-анимация для CRITICAL КПИ через `lv_anim_*`
