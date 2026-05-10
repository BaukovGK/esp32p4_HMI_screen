/**
 * @file plant_data.h
 * @brief Централизованное хранилище данных установки обратного осмоса.
 *
 * Содержит все структуры данных для хранения текущего состояния установки:
 * датчики давления, расхода, проводимости, температуры, состояние автоматики,
 * промывки, дозатора, блокировок, диагностики и настроек.
 *
 * Потокобезопасность обеспечивается внутренним мьютексом FreeRTOS.
 * Задача MQTT записывает данные через потокобезопасные сеттеры,
 * задача UI читает данные между вызовами lock/unlock.
 *
 * Механизм "грязных флагов" (dirty_flags) позволяет UI определить,
 * какие группы данных были обновлены с момента последнего чтения.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* ---- Перечисления состояний установки ---- */

/** Основное состояние установки */
typedef enum {
    PLANT_STATE_IDLE = 0,       // Ожидание (установка остановлена)
    PLANT_STATE_AUTO,           // Автоматический режим работы
    PLANT_STATE_WASHING,        // Режим химической промывки мембран
    PLANT_STATE_MANUAL,         // Ручное управление
    PLANT_STATE_FAULT,          // Аварийный останов
    PLANT_STATE_UNKNOWN,        // Состояние неизвестно (нет связи)
} plant_state_t;

/** Подсостояния автоматического режима (последовательный пуск насосов) */
typedef enum {
    AUTO_SUB_NONE = 0,          // Нет подсостояния
    AUTO_SUB_STARTING_PUMP1,    // Запуск насоса 1 (подача исходной воды)
    AUTO_SUB_RAMP,              // Набор давления (разгон)
    AUTO_SUB_STARTING_PUMP2,    // Запуск насоса 2 (дозирование / промежуточный)
    AUTO_SUB_FILLING_INTERM,    // Заполнение промежуточной ёмкости
    AUTO_SUB_STARTING_PUMP3,    // Запуск насоса 3 (вторая ступень)
    AUTO_SUB_RUNNING,           // Рабочий режим (все ступени работают)
    AUTO_SUB_STOPPING,          // Останов (последовательное выключение)
} auto_sub_state_t;

/** Подсостояния режима химической промывки */
typedef enum {
    WASH_SUB_NONE = 0,          // Нет подсостояния
    WASH_SUB_WAIT_HEAT,         // Ожидание начала нагрева
    WASH_SUB_HEATING,           // Нагрев раствора до целевой температуры
    WASH_SUB_WAIT_SUPPLY,       // Ожидание начала подачи раствора
    WASH_SUB_SUPPLY,            // Подача раствора через мембраны
    WASH_SUB_WAIT_DRAIN,        // Ожидание начала слива
    WASH_SUB_DRAIN,             // Слив промывочного раствора
    WASH_SUB_DONE,              // Промывка завершена
} wash_sub_state_t;

/** Состояние дозирующего насоса (антискалант) */
typedef enum {
    DOSER_STATE_OFF = 0,        // Дозатор выключен
    DOSER_STATE_RUNNING,        // Дозатор работает (впрыск)
    DOSER_STATE_PAUSE,          // Пауза между циклами дозирования
} doser_state_t;

/** Категория аварийного сообщения (по возрастанию критичности) */
typedef enum {
    ALARM_CAT_INFO = 0,         // Информационное сообщение
    ALARM_CAT_WARNING,          // Предупреждение (работа продолжается)
    ALARM_CAT_ALARM,            // Авария (требуется вмешательство)
    ALARM_CAT_CRITICAL,         // Критическая авария (аварийный останов)
} alarm_category_t;

/* ---- Структуры данных датчиков ---- */

/** Аналоговый канал (давление P1-P4 или температура T) */
typedef struct {
    float value;    // Значение в инженерных единицах (NAN при неисправности)
    bool  fault;    // Обрыв линии датчика (wire break)
} analog_channel_t;

/** Канал расходомера (Q1-Q4) */
typedef struct {
    float flow;     // Мгновенный расход, м3/ч (NAN при неисправности)
    float volume;   // Накопленный объём, м3
    bool  ok;       // Канал исправен
} flow_channel_t;

/** Канал кондуктометра (s1-s4: FEED, PERM1, PERM2, CONC) */
typedef struct {
    float conductivity; // Электропроводность, мкСм/см (NAN при неисправности)
    float temperature;  // Температура компенсации, градусы C
    bool  ok;           // Канал исправен
} cond_channel_t;

/** Снимок данных с цифрового счётчика электроэнергии KWS-306L (LP / HP насос). */
typedef struct {
    float voltage;       // Напряжение, В (NAN если offline)
    float current;       // Ток, А (NAN если offline)
    float power;         // Активная мощность, Вт (NAN если offline)
    float energy;        // Накопленная энергия, кВт·ч (NAN если offline)
    float temperature;   // Температура счётчика, °C (NAN если offline)
    bool  online;        // Modbus-устройство отвечает
} power_meter_data_t;

/** Расчётная телеметрия (вычисляется контроллером на основе датчиков) */
typedef struct {
    float filter_dp;    // Перепад давления на фильтре, бар (P1 - P2)
    float stage1_feed;  // Суммарная подача на 1-ю ступень, м3/ч (Q1 + Q2)
    float recovery2;    // Выход пермеата 2-й ступени, % = Q3 / (Q3+Q4) * 100
    float recovery_sys; // Общий выход системы, % = Q3 / Q1 * 100
    float sel1;         // Селективность 1-й ступени, %
    float sel2;         // Селективность 2-й ступени, %
} telemetry_t;

/** Состояние блокировок и защит */
typedef struct {
    uint32_t flags;     // Битовая маска активных блокировок
    bool     estop;     // Аварийная кнопка (грибок) нажата
    bool     filter_warn; // Предупреждение: перепад давления на фильтре превышен
} interlocks_t;

/** Состояние дозатора антискаланта */
typedef struct {
    doser_state_t state;    // Текущее состояние дозатора
    bool          enabled;  // Дозирование разрешено
} doser_status_t;

/** Диагностика контроллера ESP32-S3 */
typedef struct {
    uint32_t heap_free;         // Свободная куча, байт
    uint32_t heap_min;          // Минимум кучи за всё время работы, байт
    int64_t  uptime_s;          // Время работы контроллера, секунды
    uint32_t stack_modbus;      // Остаток стека задачи Modbus, байт
    uint32_t stack_io;          // Остаток стека задачи IO, байт
    uint32_t stack_process;     // Остаток стека задачи Process, байт
    uint32_t stack_watchdog;    // Остаток стека задачи Watchdog, байт
    uint32_t stack_mqtt;        // Остаток стека задачи MQTT, байт
    uint32_t modbus_errors[4];  // Счётчики ошибок по каждому Modbus-устройству
    bool     modbus_online[4];  // Статус связи с каждым Modbus-устройством
    uint32_t wdt_stale;         // Битовая маска: какие задачи не отвечают WDT
} diagnostics_t;

/** Запись журнала аварий */
typedef struct {
    uint32_t         id;        // Уникальный идентификатор аварии
    int64_t          ts;        // Временная метка (uptime контроллера), мкс
    alarm_category_t cat;       // Категория: INFO / WARNING / ALARM / CRITICAL
    char             code[24];  // Код аварии (например "ESTOP", "P1_HIGH", "SOURCE_EMPTY")
    float            value;     // Значение датчика в момент срабатывания
    bool             active;    // true = авария активна, false = квитирована/снята
} alarm_entry_t;

/* ---- Уставки (локальный кэш настроек контроллера) ---- */

/** Уставки давления */
typedef struct {
    float p1_max;           // Максимальное давление P1 (вход), бар
    float p3_max;           // Максимальное давление P3 (мембраны), бар
    float p4_max;           // Максимальное давление P4 (выход), бар
    float filter_dp_warn;   // Порог предупреждения перепада на фильтре, бар
} settings_pressure_t;

/** Уставки дозатора */
typedef struct {
    int run_time_min;       // Время работы дозатора в цикле, мин
    int cycle_time_min;     // Полный цикл дозирования (работа + пауза), мин
} settings_doser_t;

/** Уставки химической промывки */
typedef struct {
    float target_temp_C;    // Целевая температура раствора, градусы C
    float max_temp_C;       // Максимально допустимая температура, градусы C
    float t_overshoot_C;    // Аварийный порог перегрева, градусы C
    float hysteresis_C;     // Гистерезис регулирования нагрева, градусы C
    int   heat_timeout_min; // Таймаут нагрева (защита), мин
    int   supply_time_min;  // Время подачи раствора через мембраны, мин
    int   drain_time_min;   // Время слива раствора, мин
} settings_washing_t;

/** Уставки таймаутов */
typedef struct {
    int pump_confirm_ms;    // Таймаут подтверждения запуска насоса, мс
    int pump_ramp_ms;       // Время разгона (набора давления), мс
} settings_timeouts_t;

/* ---- Главная структура данных установки ---- */

/**
 * Единая структура, хранящая полный снимок состояния установки.
 * Доступ из нескольких задач: MQTT-задача пишет, UI-задача читает.
 * Защита: мьютекс s_mutex в plant_data.c.
 */
typedef struct {
    /* --- Состояние автоматики --- */
    plant_state_t    state;         // Основное состояние установки
    auto_sub_state_t auto_sub;      // Подсостояние автоматического режима
    wash_sub_state_t wash_sub;      // Подсостояние режима промывки
    uint32_t         fault_flags;   // Битовая маска активных аварий

    /* --- Дискретные входы/выходы --- */
    uint8_t di;         // Цифровые входы (8 бит, битовая маска)
    uint8_t do_bits;    // Цифровые выходы (8 бит, битовая маска)

    /* --- Аналоговые датчики --- */
    analog_channel_t pressure[4];   // Датчики давления P1..P4
    analog_channel_t temperature;   // Датчик температуры T

    /* --- Расходомеры --- */
    flow_channel_t flow[4];         // Расходомеры Q1..Q4

    /* --- Кондуктометры --- */
    cond_channel_t conductivity[4]; // Кондуктометры s1..s4 (FEED, PERM1, PERM2, CONC)

    /* --- Счётчики электроэнергии KWS-306L (3-фазные) --- */
    power_meter_data_t pumps[2];    // [0] = LP-насос (НД), [1] = HP-насос (ВД)

    /* --- Расчётная телеметрия --- */
    telemetry_t telemetry;

    /* --- Подсистемы --- */
    interlocks_t   interlocks;      // Блокировки и защиты
    doser_status_t doser;           // Состояние дозатора
    diagnostics_t  diagnostics;     // Диагностика контроллера

    /* --- Статус HMI-панели --- */
    bool    mqtt_connected;         // Соединение MQTT установлено
    int64_t last_msg_time_us;       // Время последнего сообщения, мкс (esp_timer)

    /* --- Кэш уставок (копия с контроллера) --- */
    settings_pressure_t set_pressure;   // Уставки давления
    settings_doser_t    set_doser;      // Уставки дозатора
    settings_washing_t  set_washing;    // Уставки промывки
    settings_timeouts_t set_timeouts;   // Уставки таймаутов

    /**
     * Грязные флаги (dirty flags).
     * Битовая маска: MQTT-сеттеры устанавливают биты при записи данных,
     * UI-задача читает и сбрасывает через plant_data_get_and_clear_dirty().
     * Позволяет перерисовывать только изменившиеся виджеты.
     */
    uint32_t dirty_flags;
} plant_data_t;

/* ---- Биты грязных флагов ---- */
#define DIRTY_STATE         (1u << 0)   // Изменилось состояние автоматики
#define DIRTY_IO            (1u << 1)   // Изменились дискретные входы/выходы
#define DIRTY_ANALOG        (1u << 2)   // Изменились аналоговые датчики (P, T)
#define DIRTY_FLOW          (1u << 3)   // Изменились данные расходомеров
#define DIRTY_CONDUCTIVITY  (1u << 4)   // Изменились данные кондуктометров
#define DIRTY_TELEMETRY     (1u << 5)   // Изменилась расчётная телеметрия
#define DIRTY_INTERLOCKS    (1u << 6)   // Изменились блокировки
#define DIRTY_DOSER         (1u << 7)   // Изменилось состояние дозатора
#define DIRTY_DIAGNOSTICS   (1u << 8)   // Изменилась диагностика
#define DIRTY_ALARMS        (1u << 9)   // Изменился журнал аварий
#define DIRTY_POWER         (1u << 10)  // Изменились данные KWS-306L (LP / HP)
#define DIRTY_ALL           (0x7FFu)    // Все флаги (биты 0..10)

/* DI — Цифровые входы (битовые маски) */
#define DI_SOURCE_LOW    (1u << 0)   /* DI1: нижний уровень исходной ёмкости  */
#define DI_SOURCE_HIGH   (1u << 1)   /* DI2: верхний уровень исходной ёмкости */
#define DI_INTERM_LOW    (1u << 2)   /* DI3: нижний уровень промежуточной     */
#define DI_INTERM_HIGH   (1u << 3)   /* DI4: верхний уровень промежуточной    */
#define DI_PUMP1_RUN     (1u << 4)   /* DI5: подтверждение работы насоса 1    */
#define DI_PUMP2_RUN     (1u << 5)   /* DI6: подтверждение работы насоса 2    */
#define DI_PUMP3_RUN     (1u << 6)   /* DI7: подтверждение работы насоса 3    */
#define DI_PERMEATE_HIGH (1u << 7)   /* DI8: верхний уровень ёмкости пермеата */

/* DO — Цифровые выходы (битовые маски) */
#define DO_PUMP1         (1u << 0)   /* Подающий насос    */
#define DO_PUMP2         (1u << 1)   /* Насос 1й ступени  */
#define DO_PUMP3         (1u << 2)   /* Насос 2й ступени  */
#define DO_HEATER        (1u << 3)   /* Нагреватель       */
#define DO_DOSER         (1u << 4)   /* Дозатор           */
#define DO_VALVE1        (1u << 5)   /* Клапан 1          */
#define DO_VALVE2        (1u << 6)   /* Клапан 2          */

/* ---- API (потокобезопасный доступ через внутренний мьютекс) ---- */

/** Инициализация хранилища: обнуление, загрузка уставок из NVS, создание мьютекса */
void plant_data_init(void);

/* ---- Локальное кэширование уставок в NVS ---- */

/** Сохранить уставки давления в NVS и обновить кэш в plant_data */
void plant_data_save_settings_pressure(const settings_pressure_t *s);

/** Сохранить уставки дозатора в NVS и обновить кэш в plant_data */
void plant_data_save_settings_doser(const settings_doser_t *s);

/** Сохранить уставки промывки в NVS и обновить кэш в plant_data */
void plant_data_save_settings_washing(const settings_washing_t *s);

/** Сохранить уставки таймаутов в NVS и обновить кэш в plant_data */
void plant_data_save_settings_timeouts(const settings_timeouts_t *s);

/**
 * Захват мьютекса для пакетного чтения (UI-задача).
 * @param timeout_ms максимальное время ожидания, мс
 * @return true если мьютекс захвачен
 */
bool plant_data_lock(uint32_t timeout_ms);

/** Освобождение мьютекса после чтения */
void plant_data_unlock(void);

/**
 * Получение указателя на данные (только для чтения).
 * ВАЖНО: вызывать только между lock/unlock!
 */
const plant_data_t *plant_data_get(void);

/**
 * Получение мутабельного указателя (для прямой модификации).
 * ВАЖНО: вызывать только между lock/unlock!
 */
plant_data_t *plant_data_get_mutable(void);

/**
 * Атомарное чтение и сброс грязных флагов.
 * ВАЖНО: вызывать только между lock/unlock!
 * @return битовая маска флагов, установленных с прошлого вызова
 */
uint32_t plant_data_get_and_clear_dirty(void);

/* ---- Потокобезопасные сеттеры (вызываются из задачи MQTT) ---- */

/** Установить состояние автоматики и флаги аварий */
void plant_data_set_state(plant_state_t state, auto_sub_state_t asub,
                          wash_sub_state_t wsub, uint32_t fault_flags);

/** Установить состояние дискретных входов/выходов */
void plant_data_set_io(uint8_t di, uint8_t do_bits);

/** Установить значение датчика давления (idx: 0=P1, 1=P2, 2=P3, 3=P4) */
void plant_data_set_pressure(int idx, float value, bool fault);

/** Установить значение датчика температуры */
void plant_data_set_temperature(float value, bool fault);

/** Установить данные расходомера (idx: 0=Q1, 1=Q2, 2=Q3, 3=Q4) */
void plant_data_set_flow(int idx, float flow, float volume, bool ok);

/** Установить данные кондуктометра (idx: 0=s1, 1=s2, 2=s3, 3=s4) */
void plant_data_set_conductivity(int idx, float cond, float temp, bool ok);

/**
 * Установить данные счётчика электроэнергии KWS-306L.
 * @param idx   0 = LP-насос (низкого давления), 1 = HP-насос (высокого давления)
 * @param data  снимок параметров (копируется внутрь plant_data)
 */
void plant_data_set_power_meter(int idx, const power_meter_data_t *data);

/** Установить расчётную телеметрию (копирование структуры) */
void plant_data_set_telemetry(const telemetry_t *tel);

/** Установить состояние блокировок */
void plant_data_set_interlocks(uint32_t flags, bool estop, bool filter_warn);

/** Установить состояние дозатора */
void plant_data_set_doser(doser_state_t state, bool enabled);

/** Установить диагностику контроллера (копирование структуры) */
void plant_data_set_diagnostics(const diagnostics_t *diag);

/** Установить статус MQTT-соединения (для отображения на HMI) */
void plant_data_set_mqtt_status(bool connected);

/* ---- Удобные геттеры (потокобезопасные, копируют под мьютексом) ---- */

/**
 * Получить копию снимка данных LP-насоса (счётчик KWS-306L).
 * Захватывает мьютекс на время копирования.
 * @return структура с данными; .online=false если timeout мьютекса
 */
power_meter_data_t plant_data_get_power_lp(void);

/**
 * Получить копию снимка данных HP-насоса (счётчик KWS-306L).
 * Захватывает мьютекс на время копирования.
 * @return структура с данными; .online=false если timeout мьютекса
 */
power_meter_data_t plant_data_get_power_hp(void);
