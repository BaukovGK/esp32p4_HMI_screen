#pragma once

/* Subscribe topics */
#define MQTT_TOPIC_STATUS_ALL       "ro_plant/status/#"
#define MQTT_TOPIC_ALARMS           "ro_plant/alarms"

/* Status topics (for routing incoming messages) */
#define MQTT_TOPIC_STATE            "ro_plant/status/state"
#define MQTT_TOPIC_IO               "ro_plant/status/io"
#define MQTT_TOPIC_ANALOG_PREFIX    "ro_plant/status/analog/"
#define MQTT_TOPIC_FLOW_PREFIX      "ro_plant/status/flow/"
#define MQTT_TOPIC_COND_PREFIX      "ro_plant/status/conductivity/"
#define MQTT_TOPIC_TELEMETRY        "ro_plant/status/telemetry"
#define MQTT_TOPIC_DOSER            "ro_plant/status/doser"
#define MQTT_TOPIC_INTERLOCKS       "ro_plant/status/interlocks"
#define MQTT_TOPIC_DIAGNOSTICS      "ro_plant/status/diagnostics"

/* Command topics (publish) */
#define MQTT_TOPIC_CMD_MODE         "ro_plant/command/mode"
#define MQTT_TOPIC_CMD_PUMP         "ro_plant/command/pump"
#define MQTT_TOPIC_CMD_DOSER        "ro_plant/command/doser"
#define MQTT_TOPIC_CMD_HEATER       "ro_plant/command/heater"

/* Settings topics (publish) */
#define MQTT_TOPIC_SET_PRESSURE     "ro_plant/settings/pressure"
#define MQTT_TOPIC_SET_DOSER        "ro_plant/settings/doser"
#define MQTT_TOPIC_SET_WASHING      "ro_plant/settings/washing"
#define MQTT_TOPIC_SET_TIMEOUTS     "ro_plant/settings/timeouts"
