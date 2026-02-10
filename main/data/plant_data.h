#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* ---- Enumerations ---- */

typedef enum {
    PLANT_STATE_IDLE = 0,
    PLANT_STATE_AUTO,
    PLANT_STATE_WASHING,
    PLANT_STATE_MANUAL,
    PLANT_STATE_FAULT,
    PLANT_STATE_UNKNOWN,
} plant_state_t;

typedef enum {
    AUTO_SUB_NONE = 0,
    AUTO_SUB_STARTING_PUMP1,
    AUTO_SUB_RAMP,
    AUTO_SUB_STARTING_PUMP2,
    AUTO_SUB_FILLING_INTERM,
    AUTO_SUB_STARTING_PUMP3,
    AUTO_SUB_RUNNING,
    AUTO_SUB_STOPPING,
} auto_sub_state_t;

typedef enum {
    WASH_SUB_NONE = 0,
    WASH_SUB_WAIT_HEAT,
    WASH_SUB_HEATING,
    WASH_SUB_WAIT_SUPPLY,
    WASH_SUB_SUPPLY,
    WASH_SUB_WAIT_DRAIN,
    WASH_SUB_DRAIN,
    WASH_SUB_DONE,
} wash_sub_state_t;

typedef enum {
    DOSER_STATE_OFF = 0,
    DOSER_STATE_RUNNING,
    DOSER_STATE_PAUSE,
} doser_state_t;

typedef enum {
    ALARM_CAT_INFO = 0,
    ALARM_CAT_WARNING,
    ALARM_CAT_ALARM,
    ALARM_CAT_CRITICAL,
} alarm_category_t;

/* ---- Data sub-structures ---- */

typedef struct {
    float value;    /* engineering value (NAN if fault) */
    bool  fault;    /* sensor wire break */
} analog_channel_t;

typedef struct {
    float flow;     /* m3/h (NAN if fault) */
    float volume;   /* m3 cumulative */
    bool  ok;       /* channel healthy */
} flow_channel_t;

typedef struct {
    float conductivity; /* uS/cm (NAN if fault) */
    float temperature;  /* deg C */
    bool  ok;           /* channel healthy */
} cond_channel_t;

typedef struct {
    float filter_dp;    /* bar, P1-P2 */
    float stage1_feed;  /* m3/h, Q1+Q2 */
    float recovery2;    /* %, Q3/(Q3+Q4)*100 */
    float recovery_sys; /* %, Q3/Q1*100 */
    float sel1;         /* %, selectivity stage 1 */
    float sel2;         /* %, selectivity stage 2 */
} telemetry_t;

typedef struct {
    uint32_t flags;     /* interlock bitmask */
    bool     estop;
    bool     filter_warn;
} interlocks_t;

typedef struct {
    doser_state_t state;
    bool          enabled;
} doser_status_t;

typedef struct {
    uint32_t heap_free;
    uint32_t heap_min;
    int64_t  uptime_s;
    uint32_t stack_modbus;
    uint32_t stack_io;
    uint32_t stack_process;
    uint32_t stack_watchdog;
    uint32_t stack_mqtt;
    uint32_t modbus_errors[4];
    bool     modbus_online[4];
    uint32_t wdt_stale;
} diagnostics_t;

typedef struct {
    uint32_t         id;
    int64_t          ts;
    alarm_category_t cat;
    char             code[24];
    float            value;
    bool             active;
} alarm_entry_t;

/* ---- Settings (local cache) ---- */

typedef struct {
    float p1_max;
    float p3_max;
    float p4_max;
    float filter_dp_warn;
} settings_pressure_t;

typedef struct {
    int run_time_min;
    int cycle_time_min;
} settings_doser_t;

typedef struct {
    float target_temp_C;
    float max_temp_C;
    float t_overshoot_C;
    float hysteresis_C;
    int   heat_timeout_min;
    int   supply_time_min;
    int   drain_time_min;
} settings_washing_t;

typedef struct {
    int pump_confirm_ms;
    int pump_ramp_ms;
} settings_timeouts_t;

/* ---- Master data structure ---- */

typedef struct {
    /* State */
    plant_state_t    state;
    auto_sub_state_t auto_sub;
    wash_sub_state_t wash_sub;
    uint32_t         fault_flags;

    /* I/O bitmasks */
    uint8_t di;         /* digital inputs (8 bits) */
    uint8_t do_bits;    /* digital outputs (8 bits) */

    /* Analog sensors */
    analog_channel_t pressure[4];   /* P1..P4 */
    analog_channel_t temperature;   /* T */

    /* Flow meters */
    flow_channel_t flow[4];         /* Q1..Q4 */

    /* Conductivity meters */
    cond_channel_t conductivity[3]; /* s1..s3 */

    /* Calculated telemetry */
    telemetry_t telemetry;

    /* Subsystems */
    interlocks_t   interlocks;
    doser_status_t doser;
    diagnostics_t  diagnostics;

    /* HMI-local status */
    bool    mqtt_connected;
    int64_t last_msg_time_us;   /* esp_timer_get_time() */

    /* Settings cache */
    settings_pressure_t set_pressure;
    settings_doser_t    set_doser;
    settings_washing_t  set_washing;
    settings_timeouts_t set_timeouts;

    /* Dirty flags (set by MQTT writers, cleared by UI reader) */
    uint32_t dirty_flags;
} plant_data_t;

/* ---- Dirty flag bits ---- */
#define DIRTY_STATE         (1u << 0)
#define DIRTY_IO            (1u << 1)
#define DIRTY_ANALOG        (1u << 2)
#define DIRTY_FLOW          (1u << 3)
#define DIRTY_CONDUCTIVITY  (1u << 4)
#define DIRTY_TELEMETRY     (1u << 5)
#define DIRTY_INTERLOCKS    (1u << 6)
#define DIRTY_DOSER         (1u << 7)
#define DIRTY_DIAGNOSTICS   (1u << 8)
#define DIRTY_ALARMS        (1u << 9)
#define DIRTY_ALL           (0x3FFu)

/* ---- API (thread-safe via internal mutex) ---- */

void plant_data_init(void);

/* Lock/unlock for bulk reads (UI side) */
bool plant_data_lock(uint32_t timeout_ms);
void plant_data_unlock(void);

/* Direct access (only between lock/unlock!) */
const plant_data_t *plant_data_get(void);
plant_data_t       *plant_data_get_mutable(void);

/* Read-and-clear dirty flags (atomic within lock) */
uint32_t plant_data_get_and_clear_dirty(void);

/* Thread-safe setters (called from MQTT task) */
void plant_data_set_state(plant_state_t state, auto_sub_state_t asub,
                          wash_sub_state_t wsub, uint32_t fault_flags);
void plant_data_set_io(uint8_t di, uint8_t do_bits);
void plant_data_set_pressure(int idx, float value, bool fault);
void plant_data_set_temperature(float value, bool fault);
void plant_data_set_flow(int idx, float flow, float volume, bool ok);
void plant_data_set_conductivity(int idx, float cond, float temp, bool ok);
void plant_data_set_telemetry(const telemetry_t *tel);
void plant_data_set_interlocks(uint32_t flags, bool estop, bool filter_warn);
void plant_data_set_doser(doser_state_t state, bool enabled);
void plant_data_set_diagnostics(const diagnostics_t *diag);
void plant_data_set_mqtt_status(bool connected);
