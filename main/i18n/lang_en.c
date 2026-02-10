#include "lang.h"

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
};
