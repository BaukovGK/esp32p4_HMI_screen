#include "plant_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <string.h>

static plant_data_t s_data;
static SemaphoreHandle_t s_mutex = NULL;

void plant_data_init(void)
{
    memset(&s_data, 0, sizeof(s_data));
    s_data.state = PLANT_STATE_UNKNOWN;

    /* Initialize analog values to NAN */
    for (int i = 0; i < 4; i++) {
        s_data.pressure[i].value = NAN;
        s_data.flow[i].flow = NAN;
        s_data.flow[i].volume = NAN;
    }
    s_data.temperature.value = NAN;
    for (int i = 0; i < 3; i++) {
        s_data.conductivity[i].conductivity = NAN;
        s_data.conductivity[i].temperature = NAN;
    }

    /* Default settings */
    s_data.set_pressure = (settings_pressure_t){
        .p1_max = 5.5f, .p3_max = 35.0f, .p4_max = 8.0f, .filter_dp_warn = 1.0f
    };
    s_data.set_doser = (settings_doser_t){
        .run_time_min = 5, .cycle_time_min = 60
    };
    s_data.set_washing = (settings_washing_t){
        .target_temp_C = 35.0f, .max_temp_C = 40.0f, .t_overshoot_C = 45.0f,
        .hysteresis_C = 2.0f, .heat_timeout_min = 30,
        .supply_time_min = 20, .drain_time_min = 5
    };
    s_data.set_timeouts = (settings_timeouts_t){
        .pump_confirm_ms = 3000, .pump_ramp_ms = 15000
    };

    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex);
}

bool plant_data_lock(uint32_t timeout_ms)
{
    return xSemaphoreTake(s_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void plant_data_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

const plant_data_t *plant_data_get(void)
{
    return &s_data;
}

plant_data_t *plant_data_get_mutable(void)
{
    return &s_data;
}

uint32_t plant_data_get_and_clear_dirty(void)
{
    uint32_t flags = s_data.dirty_flags;
    s_data.dirty_flags = 0;
    return flags;
}

/* ---- Thread-safe setters ---- */

void plant_data_set_state(plant_state_t state, auto_sub_state_t asub,
                          wash_sub_state_t wsub, uint32_t fault_flags)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.state = state;
    s_data.auto_sub = asub;
    s_data.wash_sub = wsub;
    s_data.fault_flags = fault_flags;
    s_data.dirty_flags |= DIRTY_STATE;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_io(uint8_t di, uint8_t do_bits)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.di = di;
    s_data.do_bits = do_bits;
    s_data.dirty_flags |= DIRTY_IO;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_pressure(int idx, float value, bool fault)
{
    if (idx < 0 || idx >= 4) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.pressure[idx].value = value;
    s_data.pressure[idx].fault = fault;
    s_data.dirty_flags |= DIRTY_ANALOG;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_temperature(float value, bool fault)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.temperature.value = value;
    s_data.temperature.fault = fault;
    s_data.dirty_flags |= DIRTY_ANALOG;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_flow(int idx, float flow, float volume, bool ok)
{
    if (idx < 0 || idx >= 4) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.flow[idx].flow = flow;
    s_data.flow[idx].volume = volume;
    s_data.flow[idx].ok = ok;
    s_data.dirty_flags |= DIRTY_FLOW;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_conductivity(int idx, float cond, float temp, bool ok)
{
    if (idx < 0 || idx >= 3) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.conductivity[idx].conductivity = cond;
    s_data.conductivity[idx].temperature = temp;
    s_data.conductivity[idx].ok = ok;
    s_data.dirty_flags |= DIRTY_CONDUCTIVITY;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_telemetry(const telemetry_t *tel)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.telemetry = *tel;
    s_data.dirty_flags |= DIRTY_TELEMETRY;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_interlocks(uint32_t flags, bool estop, bool filter_warn)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.interlocks.flags = flags;
    s_data.interlocks.estop = estop;
    s_data.interlocks.filter_warn = filter_warn;
    s_data.dirty_flags |= DIRTY_INTERLOCKS;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_doser(doser_state_t state, bool enabled)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.doser.state = state;
    s_data.doser.enabled = enabled;
    s_data.dirty_flags |= DIRTY_DOSER;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_diagnostics(const diagnostics_t *diag)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.diagnostics = *diag;
    s_data.dirty_flags |= DIRTY_DIAGNOSTICS;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

void plant_data_set_mqtt_status(bool connected)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_data.mqtt_connected = connected;
    s_data.dirty_flags |= DIRTY_STATE;
    xSemaphoreGive(s_mutex);
}
