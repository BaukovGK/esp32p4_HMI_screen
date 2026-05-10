/* ─────────────────────────────────────────────────────────────────────────
 * RO Plant HMI — общий JS прототипа
 * - i18n: data-i18n="key" + словарь dict.ru/en
 * - theme: <html data-theme="light|dark">, persist в localStorage
 * - language: persist в localStorage
 * - mock data: имитация живых параметров с jitter
 * - clock: обновление времени в status bar
 * ─────────────────────────────────────────────────────────────────────── */

/* ───── словарь ───── */
const i18n = {
    ru: {
        // status bar
        'sb.online': 'Связь с хабом',
        'sb.offline': 'Нет связи',
        'sb.theme': 'Тема',
        'sb.lang': 'Язык',
        'sb.system_idle': 'Система в готовности',
        'sb.system_running': 'Установка в работе',
        'sb.system_washing': 'Идёт промывка',
        'sb.system_fault': 'АВАРИЯ',

        // tab bar
        'tab.main': 'Главная',
        'tab.washing': 'Промывка',
        'tab.settings': 'Настройки',
        'tab.debug': 'Отладка',

        // mode badges
        'mode.idle': 'Ожидание',
        'mode.auto': 'Авто',
        'mode.washing': 'Мойка',
        'mode.manual': 'Ручной',
        'mode.fault': 'Авария',

        // main screen
        'main.title': 'Технологическая схема',
        'main.feed_tank': 'Исходная',
        'main.inter_tank': 'Промеж.',
        'main.product_tank': 'Чистая',
        'main.pump_pre': 'Преднагн.',
        'main.pump_st1': '1-я ст.',
        'main.pump_st2': '2-я ст.',
        'main.filter': 'Фильтр',
        'main.membrane1': 'Мембрана 1ст.',
        'main.membrane2': 'Мембрана 2ст.',
        'main.doser': 'Дозатор',
        'main.start_auto': 'Пуск AUTO',
        'main.stop': 'Стоп',
        'main.washing': 'Промывка',
        'main.manual': 'Ручной режим',
        'main.silence': 'Заглушить',

        // params
        'p.feed_pressure': 'Давление преднагн.',
        'p.filter_pressure': 'После фильтра',
        'p.st1_pressure': 'Давление 1ст.',
        'p.st2_pressure': 'Давление 2ст.',
        'p.temp': 'Температура',
        'p.feed_flow': 'Расход вход',
        'p.recycle_flow': 'Рецикл 1ст.',
        'p.product_flow': 'Расход пермеат',
        'p.conc_flow': 'Концентрат 2ст.',
        'p.feed_cond': 'σ исходная',
        'p.perm1_cond': 'σ пермеат 1ст.',
        'p.perm2_cond': 'σ пермеат 2ст.',
        'p.conc_cond': 'σ концентрат',
        'p.lp_voltage': 'НД напр.',
        'p.lp_current': 'НД ток',
        'p.lp_power': 'НД мощн.',
        'p.lp_energy': 'НД энергия',
        'p.hp_voltage': 'ВД напр.',
        'p.hp_current': 'ВД ток',
        'p.hp_power': 'ВД мощн.',
        'p.hp_energy': 'ВД энергия',

        // washing
        'wash.title': 'Режим промывки',
        'wash.phase_idle': 'Ожидание',
        'wash.phase_heating': 'Нагрев',
        'wash.phase_circulation': 'Подача',
        'wash.phase_drain': 'Слив',
        'wash.target_temp': 'Целевая темп.',
        'wash.current_temp': 'Текущая темп.',
        'wash.elapsed': 'Прошло',
        'wash.remaining': 'Осталось',
        'wash.start_heat': 'Начать нагрев',
        'wash.confirm_circ': 'Подтвердить подачу',
        'wash.confirm_drain': 'Подтвердить слив',
        'wash.cancel': 'Отменить мойку',

        // settings
        'set.title': 'Настройки и уставки',
        'set.pressure': 'Давление',
        'set.doser': 'Дозатор',
        'set.washing': 'Промывка',
        'set.timeouts': 'Таймауты',
        'set.network': 'Сеть',
        'set.about': 'О системе',
        'set.save': 'Сохранить',
        'set.cancel': 'Отмена',
        'set.p1_max': 'Макс. P1 (преднагн.)',
        'set.p3_max': 'Макс. P3 (1ст.)',
        'set.p4_max': 'Макс. P4 (2ст.)',
        'set.filter_dp': 'ΔP фильтра (warn)',
        'set.doser_run': 'Время работы',
        'set.doser_cycle': 'Период цикла',
        'set.wash_target': 'Темп. мойки',
        'set.wash_max': 'Макс. темп.',
        'set.wash_hyst': 'Гистерезис',
        'set.pump_confirm': 'Тайм-аут подтв.',
        'set.pump_ramp': 'Тайм-аут разгона',

        // debug
        'dbg.title': 'Журнал и диагностика',
        'dbg.alarms': 'Активные аварии',
        'dbg.history': 'История событий',
        'dbg.diag': 'Диагностика',
        'dbg.heap_free': 'Свободно heap',
        'dbg.heap_min': 'Мин. heap',
        'dbg.uptime': 'Время работы',
        'dbg.fw_ver': 'Версия прошивки',
        'dbg.controller': 'Контроллер',
        'dbg.modbus': 'Modbus устройства',
        'dbg.device_addr': 'Адрес',
        'dbg.device_status': 'Статус',
        'dbg.device_errors': 'Ошибок',
        'dbg.online': 'Онлайн',
        'dbg.offline': 'Офлайн',
        'dbg.clear_log': 'Очистить лог',

        // confirm
        'confirm.title': 'Подтвердите действие',
        'confirm.start_auto': 'Запустить установку в автоматическом режиме?',
        'confirm.stop': 'Остановить установку?',
        'confirm.washing': 'Перейти в режим промывки? Все насосы будут остановлены.',
        'confirm.manual': 'Перейти в ручной режим? Блокировки будут ослаблены.',
        'confirm.cancel_wash': 'Прервать цикл промывки?',
        'confirm.yes': 'Да',
        'confirm.no': 'Отмена',
    },

    en: {
        'sb.online': 'Hub connected',
        'sb.offline': 'No connection',
        'sb.theme': 'Theme',
        'sb.lang': 'Lang',
        'sb.system_idle': 'System ready',
        'sb.system_running': 'Plant running',
        'sb.system_washing': 'Cleaning in progress',
        'sb.system_fault': 'FAULT',

        'tab.main': 'Main',
        'tab.washing': 'Cleaning',
        'tab.settings': 'Settings',
        'tab.debug': 'Debug',

        'mode.idle': 'Idle',
        'mode.auto': 'Auto',
        'mode.washing': 'Wash',
        'mode.manual': 'Manual',
        'mode.fault': 'Fault',

        'main.title': 'Process diagram',
        'main.feed_tank': 'Feed',
        'main.inter_tank': 'Buffer',
        'main.product_tank': 'Product',
        'main.pump_pre': 'Pre-pump',
        'main.pump_st1': 'Stage 1',
        'main.pump_st2': 'Stage 2',
        'main.filter': 'Filter',
        'main.membrane1': 'Membrane 1',
        'main.membrane2': 'Membrane 2',
        'main.doser': 'Doser',
        'main.start_auto': 'Start AUTO',
        'main.stop': 'Stop',
        'main.washing': 'Cleaning',
        'main.manual': 'Manual mode',
        'main.silence': 'Silence',

        'p.feed_pressure': 'Pre-pump pressure',
        'p.filter_pressure': 'After filter',
        'p.st1_pressure': 'Stage 1 pressure',
        'p.st2_pressure': 'Stage 2 pressure',
        'p.temp': 'Temperature',
        'p.feed_flow': 'Feed flow',
        'p.recycle_flow': 'Recycle 1st',
        'p.product_flow': 'Permeate flow',
        'p.conc_flow': 'Concentrate 2nd',
        'p.feed_cond': 'σ feed',
        'p.perm1_cond': 'σ permeate 1',
        'p.perm2_cond': 'σ permeate 2',
        'p.conc_cond': 'σ concentrate',
        'p.lp_voltage': 'LP volt.',
        'p.lp_current': 'LP curr.',
        'p.lp_power': 'LP power',
        'p.lp_energy': 'LP energy',
        'p.hp_voltage': 'HP volt.',
        'p.hp_current': 'HP curr.',
        'p.hp_power': 'HP power',
        'p.hp_energy': 'HP energy',

        'wash.title': 'Cleaning mode',
        'wash.phase_idle': 'Idle',
        'wash.phase_heating': 'Heating',
        'wash.phase_circulation': 'Circulation',
        'wash.phase_drain': 'Drain',
        'wash.target_temp': 'Target temp.',
        'wash.current_temp': 'Current temp.',
        'wash.elapsed': 'Elapsed',
        'wash.remaining': 'Remaining',
        'wash.start_heat': 'Start heating',
        'wash.confirm_circ': 'Confirm circulation',
        'wash.confirm_drain': 'Confirm drain',
        'wash.cancel': 'Cancel cleaning',

        'set.title': 'Settings & setpoints',
        'set.pressure': 'Pressure',
        'set.doser': 'Doser',
        'set.washing': 'Cleaning',
        'set.timeouts': 'Timeouts',
        'set.network': 'Network',
        'set.about': 'About',
        'set.save': 'Save',
        'set.cancel': 'Cancel',
        'set.p1_max': 'P1 max (pre-pump)',
        'set.p3_max': 'P3 max (stage 1)',
        'set.p4_max': 'P4 max (stage 2)',
        'set.filter_dp': 'Filter ΔP (warn)',
        'set.doser_run': 'Run time',
        'set.doser_cycle': 'Cycle period',
        'set.wash_target': 'Wash target temp.',
        'set.wash_max': 'Max temp.',
        'set.wash_hyst': 'Hysteresis',
        'set.pump_confirm': 'Confirm timeout',
        'set.pump_ramp': 'Ramp timeout',

        'dbg.title': 'Log & diagnostics',
        'dbg.alarms': 'Active alarms',
        'dbg.history': 'Event history',
        'dbg.diag': 'Diagnostics',
        'dbg.heap_free': 'Free heap',
        'dbg.heap_min': 'Min heap',
        'dbg.uptime': 'Uptime',
        'dbg.fw_ver': 'Firmware version',
        'dbg.controller': 'Controller',
        'dbg.modbus': 'Modbus devices',
        'dbg.device_addr': 'Address',
        'dbg.device_status': 'Status',
        'dbg.device_errors': 'Errors',
        'dbg.online': 'Online',
        'dbg.offline': 'Offline',
        'dbg.clear_log': 'Clear log',

        'confirm.title': 'Confirm action',
        'confirm.start_auto': 'Start the plant in AUTO mode?',
        'confirm.stop': 'Stop the plant?',
        'confirm.washing': 'Switch to cleaning mode? All pumps will be stopped.',
        'confirm.manual': 'Switch to manual mode? Interlocks will be relaxed.',
        'confirm.cancel_wash': 'Abort cleaning cycle?',
        'confirm.yes': 'Yes',
        'confirm.no': 'Cancel',
    }
};

/* ───── theme ───── */
function applyTheme(theme) {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('hmi.theme', theme);
    const btn = document.querySelector('[data-action="toggle-theme"]');
    if (btn) btn.textContent = theme === 'dark' ? '☀' : '☾';
}
function toggleTheme() {
    const cur = document.documentElement.getAttribute('data-theme') || 'light';
    applyTheme(cur === 'light' ? 'dark' : 'light');
}

/* ───── language ───── */
function applyLang(lang) {
    localStorage.setItem('hmi.lang', lang);
    document.documentElement.lang = lang;
    document.querySelectorAll('[data-i18n]').forEach(el => {
        const key = el.getAttribute('data-i18n');
        const txt = i18n[lang][key];
        if (txt !== undefined) el.textContent = txt;
    });
    const btn = document.querySelector('[data-action="toggle-lang"]');
    if (btn) btn.textContent = lang.toUpperCase();
}
function toggleLang() {
    const cur = localStorage.getItem('hmi.lang') || 'ru';
    applyLang(cur === 'ru' ? 'en' : 'ru');
}

/* ───── clock ───── */
function startClock() {
    const update = () => {
        const el = document.querySelector('.sb-time');
        if (!el) return;
        const now = new Date();
        const hh = String(now.getHours()).padStart(2, '0');
        const mm = String(now.getMinutes()).padStart(2, '0');
        const ss = String(now.getSeconds()).padStart(2, '0');
        el.textContent = `${hh}:${mm}:${ss}`;
    };
    update();
    setInterval(update, 1000);
}

/* ───── confirm modal ───── */
function showConfirm(textKey, onYes) {
    const m = document.getElementById('confirm-modal');
    if (!m) return;
    m.querySelector('.modal-text').textContent = i18n[localStorage.getItem('hmi.lang') || 'ru'][textKey] || textKey;
    m.classList.add('open');
    const yes = m.querySelector('[data-modal="yes"]');
    const no = m.querySelector('[data-modal="no"]');
    const cleanup = () => {
        m.classList.remove('open');
        yes.removeEventListener('click', okHandler);
        no.removeEventListener('click', cleanup);
    };
    const okHandler = () => { cleanup(); if (onYes) onYes(); };
    yes.addEventListener('click', okHandler);
    no.addEventListener('click', cleanup);
}

/* ───── mock data jitter (имитация живых параметров) ───── */
function startMockData() {
    const params = document.querySelectorAll('[data-mock]');
    if (!params.length) return;
    const tick = () => {
        params.forEach(el => {
            const base = parseFloat(el.dataset.mockBase || el.dataset.mock);
            const noise = parseFloat(el.dataset.mockNoise || '0.02');
            const decimals = parseInt(el.dataset.mockDecimals || '2');
            const v = base + (Math.random() - 0.5) * 2 * noise * Math.abs(base || 1);
            el.textContent = v.toFixed(decimals);
        });
    };
    tick();
    setInterval(tick, 1000);
}

/* ───── sensor metadata (для подробного модала по клику) ───── */
const sensorMeta = {
    P1: {
        name_ru: 'Давление после насоса преднагнетания',
        name_en: 'Pressure after pre-pump',
        unit: 'bar', range: [0, 6],
        warn_at: 5.0, alarm_at: 5.5,
        modbus: 'Slave 1 · AI1 · 4–20 mA',
        location: 'после насоса преднагнетания (RO1)',
        loc_en: 'after pre-pump (RO1)',
    },
    P2: {
        name_ru: 'Давление после фильтра мехочистки',
        name_en: 'Pressure after mechanical filter',
        unit: 'bar', range: [0, 6],
        warn_at: 4.5, alarm_at: 5.0,
        modbus: 'Slave 1 · AI2 · 4–20 mA',
        location: 'после фильтра мехочистки (контроль засорения)',
        loc_en: 'after mechanical filter (clogging detection)',
    },
    P3: {
        name_ru: 'Давление после насоса 1-й ступени',
        name_en: 'Pressure after stage-1 pump',
        unit: 'bar', range: [0, 40],
        warn_at: 32, alarm_at: 35,
        modbus: 'Slave 1 · AI3 · 4–20 mA',
        location: 'между насосом 1-й ст. (RO2) и мембраной 1-й ст.',
        loc_en: 'between stage-1 pump (RO2) and membrane 1',
    },
    P4: {
        name_ru: 'Давление после насоса 2-й ступени',
        name_en: 'Pressure after stage-2 pump',
        unit: 'bar', range: [0, 10],
        warn_at: 7.5, alarm_at: 8.0,
        modbus: 'Slave 1 · AI4 · 4–20 mA',
        location: 'между насосом 2-й ст. (RO3) и мембраной 2-й ст.',
        loc_en: 'between stage-2 pump (RO3) and membrane 2',
    },
    Q1: {
        name_ru: 'Расход питательной воды',
        name_en: 'Feed water flow',
        unit: 'м³/ч', range: [0, 5],
        modbus: 'Slave 2 (УРЖ2КМ) · канал 1',
        location: 'на трубе после насоса преднагнетания',
        loc_en: 'on pipe after pre-pump',
    },
    Q2: {
        name_ru: 'Расход рецикла 1-й ступени',
        name_en: 'Stage-1 recycle flow',
        unit: 'м³/ч', range: [0, 3],
        modbus: 'Slave 2 (УРЖ2КМ) · канал 2',
        location: 'на трубе рецикла концентрата к насосу 1-й ст.',
        loc_en: 'recycle pipe back to stage-1 pump',
    },
    Q3: {
        name_ru: 'Расход товарного пермеата',
        name_en: 'Product permeate flow',
        unit: 'м³/ч', range: [0, 3],
        warn_at: 0.8,  /* предупреждение если ниже */
        modbus: 'Slave 2 (УРЖ2КМ) · канал 3',
        location: 'на трубе пермеата 2-й ступени к чистой ёмкости',
        loc_en: 'product permeate pipe to product tank',
    },
    Q4: {
        name_ru: 'Расход рецикла 2-й ступени',
        name_en: 'Stage-2 recycle flow',
        unit: 'м³/ч', range: [0, 2],
        modbus: 'Slave 2 (УРЖ2КМ) · канал 4',
        location: 'на трубе рецикла концентрата к насосу 2-й ст.',
        loc_en: 'recycle pipe back to stage-2 pump',
    },
    'σ2': {
        name_ru: 'Проводимость пермеата 1-й ступени',
        name_en: 'Stage-1 permeate conductivity',
        unit: 'мкСм/см', range: [0, 200],
        warn_at: 30, alarm_at: 50,
        modbus: 'Slave 10 (СЛ21-201) · канал X2',
        location: 'на трубе пермеата мембраны 1-й ст. вниз к промежуточной',
        loc_en: 'permeate of stage-1 membrane down to buffer tank',
    },
    'σ3': {
        name_ru: 'Проводимость товарного пермеата',
        name_en: 'Product permeate conductivity',
        unit: 'мкСм/см', range: [0, 50],
        warn_at: 5, alarm_at: 10,
        modbus: 'Slave 11 (СЛ21-101) · канал X1',
        location: 'на трубе пермеата 2-й ст. перед чистой ёмкостью',
        loc_en: 'permeate of stage-2 membrane before product tank',
    },
};

function getCurrentValueForSensor(tag) {
    /* Ищем sc-value внутри SVG sensor-circle для tag, чтобы взять
     * актуальное значение (mock-data его постоянно обновляет). */
    const groups = document.querySelectorAll('.mnemo-svg .sensor-group');
    for (const g of groups) {
        const tagEl = g.querySelector('.sc-tag');
        if (tagEl && tagEl.textContent.trim() === tag) {
            const v = g.querySelector('.sc-value');
            return v ? parseFloat(v.textContent) : NaN;
        }
    }
    return NaN;
}

function classifyValue(value, meta) {
    if (isNaN(value)) return 'offline';
    if (meta.alarm_at !== undefined && value >= meta.alarm_at) return 'danger';
    if (meta.warn_at !== undefined && value >= meta.warn_at) return 'warn';
    return 'ok';
}

function renderTrendSvg(min, max, count) {
    /* Простая псевдо-кривая для прототипа: рендерим N точек от min до max с
     * случайным шумом. На реальном устройстве это будет ring buffer 3ч. */
    const W = 660, H = 80;
    const pts = [];
    for (let i = 0; i < count; i++) {
        const t = i / (count - 1);
        const baseline = min + (max - min) * (0.4 + 0.5 * Math.sin(t * Math.PI * 1.5));
        const noise = (Math.random() - 0.5) * (max - min) * 0.05;
        pts.push({ x: t * W, y: H - ((baseline + noise - min) / (max - min)) * H });
    }
    const path = pts.map((p, i) => (i === 0 ? 'M' : 'L') + p.x.toFixed(1) + ',' + p.y.toFixed(1)).join(' ');
    return `<svg width="100%" height="100%" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none">
        <path d="${path}" stroke="var(--accent)" stroke-width="1.5" fill="none"/>
        <path d="${path} L${W},${H} L0,${H} Z" fill="var(--accent-mute)" opacity="0.5"/>
    </svg>`;
}

function showSensorDetail(tag) {
    const meta = sensorMeta[tag];
    if (!meta) return;
    const lang = localStorage.getItem('hmi.lang') || 'ru';
    const current = getCurrentValueForSensor(tag);
    const state = classifyValue(current, meta);

    const m = document.getElementById('sensor-modal');
    if (!m) return;

    const name = lang === 'ru' ? meta.name_ru : meta.name_en;
    const loc = lang === 'ru' ? meta.location : meta.loc_en;
    const stateLabel = { ok: 'Норма', warn: 'Предупреждение', danger: 'АВАРИЯ', offline: 'Нет данных' }[state];

    /* Pointer position на range bar */
    const [rmin, rmax] = meta.range;
    const pointerPct = Math.max(0, Math.min(100, ((current - rmin) / (rmax - rmin)) * 100));

    /* Уставки */
    let setpointsHtml = `<div class="sm-prop-row"><span class="key">Норма</span><span class="val">< ${meta.warn_at ?? rmax} ${meta.unit}</span></div>`;
    if (meta.warn_at) {
        setpointsHtml += `<div class="sm-prop-row"><span class="key">Предупреждение</span><span class="val">${meta.warn_at}–${meta.alarm_at ?? rmax} ${meta.unit}</span></div>`;
    }
    if (meta.alarm_at) {
        setpointsHtml += `<div class="sm-prop-row"><span class="key">АВАРИЯ</span><span class="val">> ${meta.alarm_at} ${meta.unit}</span></div>`;
    }

    /* Тренд stats — псевдо-данные для прототипа */
    const trendMin = (current * 0.85).toFixed(1);
    const trendMax = (current * 1.08).toFixed(1);
    const trendAvg = (current * 0.96).toFixed(1);

    m.querySelector('.sensor-modal').innerHTML = `
        <div class="sm-header">
            <div>
                <div class="sm-title">${tag} — ${name}</div>
                <div class="sm-subtitle">${loc}</div>
            </div>
            <button class="sm-close" data-modal="close">×</button>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Текущее значение</div>
            <div class="sm-current ${state}">
                <span class="sm-current-value">${isNaN(current) ? '—' : current.toFixed(1)}</span>
                <span class="sm-current-unit">${meta.unit}</span>
                <span class="badge badge-${state === 'ok' ? 'ok' : state === 'warn' ? 'warn' : state === 'danger' ? 'danger' : 'mute'}" style="margin-left: auto;">${stateLabel}</span>
            </div>
            <div class="sm-range-bar">
                <div class="pointer" style="left: ${pointerPct}%;"></div>
            </div>
            <div class="sm-range-labels">
                <span>${rmin} ${meta.unit}</span>
                <span>${rmax} ${meta.unit}</span>
            </div>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Тренд за 3 часа</div>
            <div class="sm-trend">${renderTrendSvg(rmin, current * 1.1, 60)}</div>
            <div class="sm-trend-stats">
                <span>Мин: <span class="stat-value">${trendMin} ${meta.unit}</span></span>
                <span>Среднее: <span class="stat-value">${trendAvg} ${meta.unit}</span></span>
                <span>Макс: <span class="stat-value">${trendMax} ${meta.unit}</span></span>
            </div>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Уставки</div>
            ${setpointsHtml}
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Источник</div>
            <div class="sm-prop-row"><span class="key">Modbus</span><span class="val">${meta.modbus}</span></div>
            <div class="sm-prop-row"><span class="key">Обновлено</span><span class="val">сейчас</span></div>
        </div>

        <div class="modal-actions">
            <button class="btn" data-modal="close">Закрыть</button>
        </div>
    `;

    /* Open + close handlers */
    m.classList.add('open');
    m.querySelectorAll('[data-modal="close"]').forEach(btn => {
        btn.addEventListener('click', () => m.classList.remove('open'), { once: true });
    });
}

/* ═══════════════════════════════════════════════════════════════════════
 * EQUIPMENT DETAIL MODALS
 * Открываются по клику на фильтр, насосы и мембраны на мнемосхеме.
 * Используют тот же #sensor-modal (только innerHTML переписывается).
 *
 * Состав словаря equipmentMeta:
 *   filter / pump-pre / pump-st1 / pump-st2 / membrane-1 / membrane-2
 *
 * Для прототипа значения наработки/ΔP — псевдо-данные. На целевом
 * устройстве эти поля будут заполняться из контроллера через MQTT
 * (топик ro/<id>/runtime, ro/<id>/state).
 * ═══════════════════════════════════════════════════════════════════════ */

const equipmentMeta = {
    'filter': {
        kind: 'filter',
        name_ru: 'Фильтр механической очистки',
        name_en: 'Mechanical filter',
        subtitle_ru: '5 мкм картридж',
        subtitle_en: '5 μm cartridge',
        delta_p: 0.32,           // bar (P1 - P2)
        delta_p_warn: 0.5,
        delta_p_alarm: 0.8,
        runtime_h: 1284,         // часы наработки картриджа
        runtime_max_h: 2000,     // ресурс картриджа
        last_replaced: '2026-03-15',
    },
    'pump-pre': {
        kind: 'pump',
        name_ru: 'Преднагнетательный насос',
        name_en: 'Pre-feed pump',
        subtitle_ru: 'RO1 · DI6',
        subtitle_en: 'RO1 · DI6',
        model: 'Grundfos CR 3-4',
        rated_q: '4 м³/ч',
        rated_p: '4 bar',
        state: 'running',
        run_continuous_h: 8.4,
        run_total_h: 4521,
        starts: 1247,
        last_start: '2026-05-10 06:08',
    },
    'pump-st1': {
        kind: 'pump',
        name_ru: 'Насос 1-й ступени',
        name_en: 'Stage-1 pump',
        subtitle_ru: 'RO2 · DI7',
        subtitle_en: 'RO2 · DI7',
        model: 'Grundfos BMG 4-15',
        rated_q: '4 м³/ч',
        rated_p: '30 bar',
        state: 'running',
        run_continuous_h: 8.4,
        run_total_h: 4380,
        starts: 1108,
        last_start: '2026-05-10 06:08',
    },
    'pump-st2': {
        kind: 'pump',
        name_ru: 'Насос 2-й ступени',
        name_en: 'Stage-2 pump',
        subtitle_ru: 'RO3 · DI8',
        subtitle_en: 'RO3 · DI8',
        model: 'Grundfos CRN 5-12',
        rated_q: '2.5 м³/ч',
        rated_p: '8 bar',
        state: 'running',
        run_continuous_h: 8.4,
        run_total_h: 4290,
        starts: 1095,
        last_start: '2026-05-10 06:08',
    },
    'membrane-1': {
        kind: 'membrane',
        name_ru: 'Мембрана 1-й ступени',
        name_en: 'Stage-1 membrane',
        subtitle_ru: '2 элемента 4040',
        subtitle_en: '2 × 4040 elements',
        type_ru: 'Обратноосмотический модуль (BW)',
        type_en: 'RO module (BW)',
        rejection: 99.2,         // % NaCl
        last_wash: '2026-04-12',
        run_since_wash_h: 678,
        run_since_wash_warn_h: 720,   // плановая промывка после ~30 дней
        run_total_h: 9412,
        run_max_h: 20000,        // ресурс мембраны
    },
    'membrane-2': {
        kind: 'membrane',
        name_ru: 'Мембрана 2-й ступени',
        name_en: 'Stage-2 membrane',
        subtitle_ru: '2 элемента 4040',
        subtitle_en: '2 × 4040 elements',
        type_ru: 'Обратноосмотический модуль (BW)',
        type_en: 'RO module (BW)',
        rejection: 99.5,
        last_wash: '2026-04-12',
        run_since_wash_h: 678,
        run_since_wash_warn_h: 720,
        run_total_h: 9412,
        run_max_h: 20000,
    },
};

/* Часы → "1284 ч (53 дн)" */
function formatHours(h) {
    if (h == null || isNaN(h)) return '—';
    if (h < 24) return `${h.toFixed(1)} ч`;
    const days = Math.floor(h / 24);
    return `${h.toFixed(0)} ч (${days} дн)`;
}

/* Прогресс-бар [0..100%] с цветом по состоянию */
function renderProgressBar(pct, state /* 'ok'|'warn'|'danger' */) {
    const color = state === 'danger' ? 'var(--danger)' :
                  state === 'warn'   ? 'var(--warning)' : 'var(--accent)';
    const w = Math.max(0, Math.min(100, pct));
    return `
        <div class="sm-range-bar" style="background: var(--bg-mute); height: 8px; border-radius: 4px; overflow: hidden;">
            <div style="width: ${w}%; height: 100%; background: ${color}; transition: width 0.4s ease;"></div>
        </div>
    `;
}

function classifyByThresholds(value, warn, alarm) {
    if (alarm != null && value >= alarm) return 'danger';
    if (warn  != null && value >= warn)  return 'warn';
    return 'ok';
}

/* ─── ФИЛЬТР ─────────────────────────────────────────────────── */
function renderFilterModal(meta, lang) {
    const name = lang === 'ru' ? meta.name_ru : meta.name_en;
    const sub  = lang === 'ru' ? meta.subtitle_ru : meta.subtitle_en;
    const dpState = classifyByThresholds(meta.delta_p, meta.delta_p_warn, meta.delta_p_alarm);
    const dpStateLabel = { ok: 'НОРМА', warn: 'ПРЕДУПРЕЖДЕНИЕ', danger: 'АВАРИЯ' }[dpState];
    const dpBadge = { ok: 'ok', warn: 'warn', danger: 'danger' }[dpState];

    const runtimePct = (meta.runtime_h / meta.runtime_max_h) * 100;
    const runtimeState = runtimePct >= 95 ? 'danger' : runtimePct >= 80 ? 'warn' : 'ok';
    const remainingH = Math.max(0, meta.runtime_max_h - meta.runtime_h);

    return `
        <div class="sm-header">
            <div>
                <div class="sm-title">Фильтр — ${name}</div>
                <div class="sm-subtitle">${sub}</div>
            </div>
            <button class="sm-close" data-modal="close">×</button>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">ΔP — перепад давления (P1 − P2)</div>
            <div class="sm-current ${dpState}">
                <span class="sm-current-value">${meta.delta_p.toFixed(2)}</span>
                <span class="sm-current-unit">bar</span>
                <span class="badge badge-${dpBadge}" style="margin-left:auto;">${dpStateLabel}</span>
            </div>
            <div class="sm-prop-row"><span class="key">Норма</span><span class="val">< ${meta.delta_p_warn} bar</span></div>
            <div class="sm-prop-row"><span class="key">Предупреждение</span><span class="val">${meta.delta_p_warn}–${meta.delta_p_alarm} bar</span></div>
            <div class="sm-prop-row"><span class="key">АВАРИЯ (засорение)</span><span class="val">> ${meta.delta_p_alarm} bar</span></div>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Наработка картриджа</div>
            <div class="sm-current ${runtimeState}">
                <span class="sm-current-value">${formatHours(meta.runtime_h)}</span>
                <span class="sm-current-unit">/ ${formatHours(meta.runtime_max_h)}</span>
                <span class="badge badge-${runtimeState === 'ok' ? 'ok' : runtimeState === 'warn' ? 'warn' : 'danger'}" style="margin-left:auto;">${runtimePct.toFixed(0)}%</span>
            </div>
            ${renderProgressBar(runtimePct, runtimeState)}
            <div class="sm-prop-row"><span class="key">До замены</span><span class="val">${formatHours(remainingH)}</span></div>
            <div class="sm-prop-row"><span class="key">Установлен</span><span class="val">${meta.last_replaced}</span></div>
        </div>

        <div class="modal-actions">
            <button class="btn" data-modal="close">Закрыть</button>
        </div>
    `;
}

/* ─── НАСОС ──────────────────────────────────────────────────── */
function renderPumpModal(meta, lang) {
    const name = lang === 'ru' ? meta.name_ru : meta.name_en;
    const sub  = lang === 'ru' ? meta.subtitle_ru : meta.subtitle_en;
    const stateLabel = {
        running:  { ru: 'РАБОТАЕТ',     en: 'RUNNING',   badge: 'ok'    },
        starting: { ru: 'ПУСК',         en: 'STARTING',  badge: 'warn'  },
        error:    { ru: 'АВАРИЯ',       en: 'ERROR',     badge: 'danger'},
        off:      { ru: 'ОСТАНОВЛЕН',   en: 'STOPPED',   badge: 'mute'  },
    }[meta.state] || { ru: '—', en: '—', badge: 'mute' };

    return `
        <div class="sm-header">
            <div>
                <div class="sm-title">${name}</div>
                <div class="sm-subtitle">${sub} · ${meta.model}</div>
            </div>
            <button class="sm-close" data-modal="close">×</button>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Состояние</div>
            <div class="sm-current ${meta.state === 'running' ? 'ok' : meta.state === 'error' ? 'danger' : 'warn'}">
                <span class="sm-current-value" style="font-size: 22px;">${lang === 'ru' ? stateLabel.ru : stateLabel.en}</span>
                <span class="badge badge-${stateLabel.badge}" style="margin-left:auto;">${meta.state.toUpperCase()}</span>
            </div>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Время работы</div>
            <div class="sm-prop-row"><span class="key">В текущем цикле</span><span class="val">${formatHours(meta.run_continuous_h)}</span></div>
            <div class="sm-prop-row"><span class="key">Запуск цикла</span><span class="val">${meta.last_start}</span></div>
            <div class="sm-prop-row"><span class="key">Общая наработка</span><span class="val">${formatHours(meta.run_total_h)}</span></div>
            <div class="sm-prop-row"><span class="key">Количество пусков</span><span class="val">${meta.starts}</span></div>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Характеристики</div>
            <div class="sm-prop-row"><span class="key">Модель</span><span class="val">${meta.model}</span></div>
            <div class="sm-prop-row"><span class="key">Q ном</span><span class="val">${meta.rated_q}</span></div>
            <div class="sm-prop-row"><span class="key">P ном</span><span class="val">${meta.rated_p}</span></div>
        </div>

        <div class="modal-actions">
            <button class="btn" data-modal="close">Закрыть</button>
        </div>
    `;
}

/* ─── МЕМБРАНА ───────────────────────────────────────────────── */
function renderMembraneModal(meta, lang) {
    const name = lang === 'ru' ? meta.name_ru : meta.name_en;
    const sub  = lang === 'ru' ? meta.subtitle_ru : meta.subtitle_en;
    const type = lang === 'ru' ? meta.type_ru  : meta.type_en;

    /* Наработка от последней промывки: warn после run_since_wash_warn_h */
    const washPct = (meta.run_since_wash_h / meta.run_since_wash_warn_h) * 100;
    const washState = washPct >= 100 ? 'danger' : washPct >= 80 ? 'warn' : 'ok';
    const washRemainingH = Math.max(0, meta.run_since_wash_warn_h - meta.run_since_wash_h);

    /* Общая наработка vs ресурс */
    const totalPct = (meta.run_total_h / meta.run_max_h) * 100;
    const totalState = totalPct >= 95 ? 'danger' : totalPct >= 80 ? 'warn' : 'ok';
    const totalRemainingH = Math.max(0, meta.run_max_h - meta.run_total_h);

    return `
        <div class="sm-header">
            <div>
                <div class="sm-title">${name}</div>
                <div class="sm-subtitle">${sub}</div>
            </div>
            <button class="sm-close" data-modal="close">×</button>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Селективность по NaCl (rejection)</div>
            <div class="sm-current ok">
                <span class="sm-current-value">${meta.rejection.toFixed(1)}</span>
                <span class="sm-current-unit">%</span>
                <span class="badge badge-ok" style="margin-left:auto;">НОРМА</span>
            </div>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Наработка от последней промывки</div>
            <div class="sm-current ${washState}">
                <span class="sm-current-value">${formatHours(meta.run_since_wash_h)}</span>
                <span class="sm-current-unit">/ ${formatHours(meta.run_since_wash_warn_h)}</span>
                <span class="badge badge-${washState === 'ok' ? 'ok' : washState === 'warn' ? 'warn' : 'danger'}" style="margin-left:auto;">${washPct.toFixed(0)}%</span>
            </div>
            ${renderProgressBar(washPct, washState)}
            <div class="sm-prop-row"><span class="key">Последняя промывка</span><span class="val">${meta.last_wash}</span></div>
            <div class="sm-prop-row"><span class="key">До плановой промывки</span><span class="val">${formatHours(washRemainingH)}</span></div>
        </div>

        <div class="sm-section">
            <div class="sm-section-title">Общее время работы</div>
            <div class="sm-current ${totalState}">
                <span class="sm-current-value">${formatHours(meta.run_total_h)}</span>
                <span class="sm-current-unit">/ ${formatHours(meta.run_max_h)}</span>
                <span class="badge badge-${totalState === 'ok' ? 'ok' : totalState === 'warn' ? 'warn' : 'danger'}" style="margin-left:auto;">${totalPct.toFixed(0)}%</span>
            </div>
            ${renderProgressBar(totalPct, totalState)}
            <div class="sm-prop-row"><span class="key">Ресурс остался</span><span class="val">${formatHours(totalRemainingH)}</span></div>
            <div class="sm-prop-row"><span class="key">Тип</span><span class="val">${type}</span></div>
        </div>

        <div class="modal-actions">
            <button class="btn" data-modal="close">Закрыть</button>
        </div>
    `;
}

/* ─── Диспетчер по типу оборудования ──────────────────────────── */
function showEquipmentDetail(equipmentId) {
    const meta = equipmentMeta[equipmentId];
    if (!meta) return;
    const lang = localStorage.getItem('hmi.lang') || 'ru';
    const m = document.getElementById('sensor-modal');
    if (!m) return;

    let html;
    if (meta.kind === 'filter')        html = renderFilterModal(meta, lang);
    else if (meta.kind === 'pump')     html = renderPumpModal(meta, lang);
    else if (meta.kind === 'membrane') html = renderMembraneModal(meta, lang);
    else return;

    m.querySelector('.sensor-modal').innerHTML = html;
    m.classList.add('open');
    m.querySelectorAll('[data-modal="close"]').forEach(btn => {
        btn.addEventListener('click', () => m.classList.remove('open'), { once: true });
    });
}

/* ───── boot ───── */
document.addEventListener('DOMContentLoaded', () => {
    applyTheme(localStorage.getItem('hmi.theme') || 'light');
    applyLang(localStorage.getItem('hmi.lang') || 'ru');
    startClock();
    startMockData();

    document.querySelectorAll('[data-action="toggle-theme"]').forEach(b => b.addEventListener('click', toggleTheme));
    document.querySelectorAll('[data-action="toggle-lang"]').forEach(b => b.addEventListener('click', toggleLang));

    // command buttons → confirm modal
    document.querySelectorAll('[data-confirm]').forEach(btn => {
        btn.addEventListener('click', () => {
            const key = btn.getAttribute('data-confirm');
            showConfirm(key, () => {
                console.log('[mock action]', btn.dataset.action || key);
            });
        });
    });

    // прямая навигация (например: кнопка Промывка → washing.html без confirm)
    document.querySelectorAll('[data-href]').forEach(btn => {
        btn.addEventListener('click', () => {
            window.location.href = btn.getAttribute('data-href');
        });
    });

    // sensor circles → детальный модал
    document.querySelectorAll('.mnemo-svg .sensor-group').forEach(g => {
        g.addEventListener('click', () => {
            const tagEl = g.querySelector('.sc-tag');
            if (!tagEl) return;
            const tag = tagEl.textContent.trim();
            showSensorDetail(tag);
        });
    });

    // equipment (filter, pumps, membranes) → детальный модал
    document.querySelectorAll('.mnemo-svg [data-equipment]').forEach(el => {
        el.addEventListener('click', e => {
            // если клик прилетел из вложенного sensor-group, не перехватываем
            if (e.target.closest('.sensor-group')) return;
            const id = el.getAttribute('data-equipment');
            showEquipmentDetail(id);
        });
    });

    // backdrop click для sensor-modal
    const sm = document.getElementById('sensor-modal');
    if (sm) {
        sm.addEventListener('click', e => {
            if (e.target === sm) sm.classList.remove('open');
        });
    }
});
