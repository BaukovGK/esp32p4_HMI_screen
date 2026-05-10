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
});
