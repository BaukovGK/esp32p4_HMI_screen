#pragma once

#include "esp_err.h"
#include "plant_data.h"
#include <stdbool.h>

esp_err_t mqtt_client_start(void);
void      mqtt_client_stop(void);
bool      mqtt_client_is_connected(void);

/* Command publishing */
esp_err_t mqtt_publish_mode_cmd(const char *cmd);
esp_err_t mqtt_publish_pump_mask(uint8_t mask);
esp_err_t mqtt_publish_doser_enable(bool enabled);
esp_err_t mqtt_publish_heater(bool on);

/* Settings publishing */
esp_err_t mqtt_publish_settings_pressure(const settings_pressure_t *s);
esp_err_t mqtt_publish_settings_doser(const settings_doser_t *s);
esp_err_t mqtt_publish_settings_washing(const settings_washing_t *s);
esp_err_t mqtt_publish_settings_timeouts(const settings_timeouts_t *s);
