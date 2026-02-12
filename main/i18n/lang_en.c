/**
 * @file lang_en.c
 * @brief Таблица строк на английском языке.
 *
 * Массив lang_en_strings[] содержит переводы всех строк интерфейса.
 * Индексация -- по значениям перечисления str_id_t (lang_strings.h).
 *
 * При добавлении новой строки: добавить элемент с designated initializer
 * в соответствующую секцию, например: [STR_NEW_ID] = "New string",
 * и обязательно добавить аналогичный перевод в lang_ru.c.
 */
#include "lang.h"

// Таблица английских строк -- вспомогательный язык интерфейса
const char *lang_en_strings[STR_COUNT] = {
    /* Mode names */
    [STR_MODE_IDLE]     = "IDLE",
    [STR_MODE_AUTO]     = "AUTO",
    [STR_MODE_WASHING]  = "WASHING",
    [STR_MODE_MANUAL]   = "MANUAL",
    [STR_MODE_FAULT]    = "FAULT",
    [STR_MODE_UNKNOWN]  = "NO LINK",

    /* Auto sub-states */
    [STR_AUTO_STARTING_PUMP1]  = "Starting feed pump",
    [STR_AUTO_RAMP]            = "Ramp-up",
    [STR_AUTO_STARTING_PUMP2]  = "Starting stage 1 pump",
    [STR_AUTO_FILLING_INTERM]  = "Filling intermediate tank",
    [STR_AUTO_STARTING_PUMP3]  = "Starting stage 2 pump",
    [STR_AUTO_RUNNING]         = "Running",
    [STR_AUTO_STOPPING]        = "Stopping",

    /* Wash sub-states */
    [STR_WASH_WAIT_HEAT]    = "Prepare for heating",
    [STR_WASH_HEATING]      = "Heating",
    [STR_WASH_WAIT_SUPPLY]  = "Prepare for supply",
    [STR_WASH_SUPPLY]       = "Supply",
    [STR_WASH_WAIT_DRAIN]   = "Prepare for drain",
    [STR_WASH_DRAIN]        = "Drain",
    [STR_WASH_DONE]         = "Done",

    /* Navigation */
    [STR_NAV_MNEMONIC]     = "P&ID",
    [STR_NAV_PARAMETERS]   = "Params",
    [STR_NAV_WASHING]      = "Washing",
    [STR_NAV_ALARMS]       = "Alarms",
    [STR_NAV_SETTINGS]     = "Settings",
    [STR_NAV_DIAGNOSTICS]  = "Diag",

    /* Buttons */
    [STR_BTN_START_AUTO]    = "START AUTO",
    [STR_BTN_STOP]          = "STOP",
    [STR_BTN_MANUAL]        = "MANUAL",
    [STR_BTN_START_WASHING] = "WASHING",
    [STR_BTN_CONFIRM]       = "CONFIRM",
    [STR_BTN_RESET_FAULT]   = "RESET",
    [STR_BTN_APPLY]         = "APPLY",

    /* Labels - equipment */
    [STR_LBL_FEED_PUMP]     = "Feed Pump",
    [STR_LBL_STAGE1_PUMP]   = "Stage 1 Pump",
    [STR_LBL_STAGE2_PUMP]   = "Stage 2 Pump",
    [STR_LBL_HEATER]        = "Heater",
    [STR_LBL_DOSER]         = "Doser",
    [STR_LBL_SOURCE_TANK]   = "Source Tank",
    [STR_LBL_INTERM_TANK]   = "Interm. Tank",
    [STR_LBL_PERMEATE_TANK] = "Permeate Tank",
    [STR_LBL_MEMBRANE_1]    = "Membrane 1",
    [STR_LBL_MEMBRANE_2]    = "Membrane 2",
    [STR_LBL_FILTER]        = "Filter",

    /* Labels - parameters */
    [STR_LBL_PRESSURE]    = "Pressure",
    [STR_LBL_TEMPERATURE] = "Temperature",
    [STR_LBL_FLOW_RATE]   = "Flow rate",
    [STR_LBL_VOLUME]      = "Volume",
    [STR_LBL_CONDUCTIVITY]= "Conductivity",
    [STR_LBL_RECOVERY]    = "Recovery",
    [STR_LBL_SELECTIVITY] = "Selectivity",
    [STR_LBL_FILTER_DP]   = "Filter dP",
    [STR_LBL_STAGE1_FEED] = "Stage 1 feed",

    /* Alarm codes */
    [STR_ALARM_ESTOP]           = "EMERGENCY STOP",
    [STR_ALARM_SOURCE_EMPTY]    = "Source tank empty",
    [STR_ALARM_INTERM_EMPTY]    = "Intermediate tank empty",
    [STR_ALARM_P1_HIGH]         = "P1 pressure high",
    [STR_ALARM_P3_HIGH]         = "P3 pressure high",
    [STR_ALARM_P4_HIGH]         = "P4 pressure high",
    [STR_ALARM_T_HIGH]          = "Overtemperature",
    [STR_ALARM_FILTER_DP]       = "Filter clogged",
    [STR_ALARM_PUMP1_TIMEOUT]   = "Feed pump timeout",
    [STR_ALARM_PUMP2_TIMEOUT]   = "Stage 1 pump timeout",
    [STR_ALARM_PUMP3_TIMEOUT]   = "Stage 2 pump timeout",
    [STR_ALARM_SENSOR_FAULT]    = "Sensor fault",
    [STR_ALARM_MQTT_DISCONNECT] = "MQTT disconnected",
    [STR_ALARM_MODBUS_OFFLINE]  = "Modbus offline",

    /* Settings sections */
    [STR_SET_PRESSURE] = "Pressure",
    [STR_SET_DOSER]    = "Doser",
    [STR_SET_WASHING]  = "Washing",
    [STR_SET_TIMEOUTS] = "Timeouts",
    [STR_SET_MQTT]     = "MQTT",

    /* Diagnostics */
    [STR_DIAG_HEAP_FREE]    = "Free heap",
    [STR_DIAG_HEAP_MIN]     = "Min heap",
    [STR_DIAG_UPTIME]       = "Uptime",
    [STR_DIAG_MQTT_STATUS]  = "MQTT status",
    [STR_DIAG_MODBUS]       = "Modbus",

    /* Status */
    [STR_STATUS_ONLINE]       = "Online",
    [STR_STATUS_OFFLINE]      = "Offline",
    [STR_STATUS_OK]           = "OK",
    [STR_STATUS_FAULT]        = "FAULT",
    [STR_STATUS_SENSOR_BREAK] = "BREAK",
    [STR_STATUS_NO_DATA]      = "No data",

    /* Confirmation dialog */
    [STR_CONFIRM_TITLE]      = "Confirmation",
    [STR_CONFIRM_START_AUTO] = "Start auto mode?",
    [STR_CONFIRM_STOP]       = "Stop the plant?",
    [STR_CONFIRM_MANUAL]     = "Switch to manual mode?",
    [STR_CONFIRM_WASHING]    = "Start washing?",
    [STR_BTN_YES]            = "YES",
    [STR_BTN_NO]             = "NO",

    /* Units */
    [STR_UNIT_BAR]     = "bar",
    [STR_UNIT_CELSIUS] = "\xC2\xB0" "C",
    [STR_UNIT_M3H]     = "m\xC2\xB3/h",
    [STR_UNIT_M3]      = "m\xC2\xB3",
    [STR_UNIT_USCM]    = "\xC2\xB5S/cm",
    [STR_UNIT_PERCENT] = "%",
    [STR_UNIT_SECONDS] = "s",
    [STR_UNIT_MINUTES] = "min",
    [STR_UNIT_BYTES]   = "B",

    /* Column headers */
    [STR_HDR_PARAMETER]     = "Parameter",
    [STR_HDR_VALUE]         = "Value",
    [STR_HDR_EXTRA]         = "Extra",
    [STR_HDR_STATUS]        = "Status",

    /* Calculated parameters */
    [STR_CALC_FILTER_DP]    = "Filter dP",
    [STR_CALC_STAGE1_FEED]  = "Stage 1 feed",
    [STR_CALC_RECOVERY2]    = "Recovery 2",
    [STR_CALC_RECOVERY_SYS] = "Recovery sys",
    [STR_CALC_SEL1]         = "Selectivity 1",
    [STR_CALC_SEL2]         = "Selectivity 2",
    [STR_CALC_SECTION]      = "Calculated",

    /* Diagnostics */
    [STR_DIAG_SYSTEM]       = "System",
    [STR_DIAG_TASK_STACKS]  = "Task Stack Watermarks",
    [STR_HDR_DEVICE]        = "Device",
    [STR_HDR_ERRORS]        = "Errors",
    [STR_HDR_TASK]          = "Task",
    [STR_HDR_FREE_BYTES]    = "Free (bytes)",

    /* Mnemonic */
    [STR_LBL_DRAIN]         = "Drain",
    [STR_LBL_RECYCLE_Q2]    = "Q2 recycle",
    [STR_LBL_RECYCLE_Q4]    = "Q4 recycle",

    /* Settings fields */
    [STR_SET_P1_MAX]        = "P1 max (bar)",
    [STR_SET_P3_MAX]        = "P3 max (bar)",
    [STR_SET_P4_MAX]        = "P4 max (bar)",
    [STR_SET_FILTER_DP_WARN]= "Filter dP warn (bar)",
    [STR_SET_RUN_TIME]      = "Run time (min)",
    [STR_SET_CYCLE_TIME]    = "Cycle time (min)",
    [STR_SET_TARGET_TEMP]   = "Target temp (\xC2\xB0" "C)",
    [STR_SET_MAX_TEMP]      = "Max temp (\xC2\xB0" "C)",
    [STR_SET_OVERSHOOT]     = "Overshoot (\xC2\xB0" "C)",
    [STR_SET_HYSTERESIS]    = "Hysteresis (\xC2\xB0" "C)",
    [STR_SET_HEAT_TIMEOUT]  = "Heat timeout (min)",
    [STR_SET_SUPPLY_TIME]   = "Supply time (min)",
    [STR_SET_DRAIN_TIME]    = "Drain time (min)",
    [STR_SET_PUMP_CONFIRM]  = "Pump confirm (ms)",
    [STR_SET_PUMP_RAMP]     = "Pump ramp (ms)",
    [STR_WASH_TARGET_INFO]  = "Target: %.1f \xC2\xB0" "C  |  Max: %.1f \xC2\xB0" "C",

    /* Alarm screen */
    [STR_ALARM_ACTIVE_TITLE]  = "Active Alarms",
    [STR_ALARM_HISTORY_TITLE] = "Alarm History",
    [STR_ALARM_NONE_ACTIVE]   = "No active alarms",
    [STR_ALARM_NONE_HISTORY]  = "No alarm history",
    [STR_ALARM_ACTIVE_BADGE]  = "ACTIVE",
};

_Static_assert(sizeof(lang_en_strings) / sizeof(lang_en_strings[0]) == STR_COUNT,
               "lang_en_strings size mismatch with str_id_t");
