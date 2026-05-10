/**
 * @file plant_data.c
 * @brief Реализация централизованного хранилища данных установки.
 *
 * Единственный экземпляр plant_data_t (s_data) хранится как статическая
 * переменная модуля. Доступ защищён мьютексом FreeRTOS (s_mutex).
 *
 * Паттерн доступа:
 *   - MQTT-задача вызывает потокобезопасные сеттеры (plant_data_set_*),
 *     которые самостоятельно захватывают/освобождают мьютекс.
 *   - UI-задача вызывает plant_data_lock(), затем читает данные через
 *     plant_data_get(), проверяет dirty_flags, и вызывает plant_data_unlock().
 *
 * Файл исключается из сборки LVGL-превью (#ifndef LVGL_LIVE_PREVIEW).
 */
#ifndef LVGL_LIVE_PREVIEW
#include "plant_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "plant_data";

// Единственный экземпляр данных установки (статический, модульная область видимости)
static plant_data_t s_data;

// Мьютекс для защиты s_data от одновременного доступа из разных задач
static SemaphoreHandle_t s_mutex = NULL;

/* Стандартный таймаут захвата мьютекса для setter/getter (мс).
 * portMAX_DELAY запрещён правилами проекта вне HAL-вызовов. */
#define PLANT_DATA_LOCK_MS 50

/**
 * Захват s_mutex с конечным таймаутом (PLANT_DATA_LOCK_MS).
 * Возвращает true при успехе, false при таймауте.
 * Используется внутри сеттеров/геттеров вместо xSemaphoreTake(.., portMAX_DELAY).
 */
static inline bool plant_data_try_lock(void)
{
    return xSemaphoreTake(s_mutex, pdMS_TO_TICKS(PLANT_DATA_LOCK_MS)) == pdTRUE;
}

/* ---- NVS: пространство имён и ключи для хранения уставок ---- */
#define NVS_NAMESPACE   "ro_settings"
#define NVS_KEY_PRESS   "set_press"
#define NVS_KEY_DOSER   "set_doser"
#define NVS_KEY_WASH    "set_wash"
#define NVS_KEY_TMOUT   "set_tmout"

/**
 * Загрузка уставок из NVS. Если ключ отсутствует — значение остаётся по умолчанию.
 * Вызывается из plant_data_init() после заполнения значений по умолчанию.
 */
static void settings_load_from_nvs(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        ESP_LOGI(TAG, "NVS: no saved settings, using defaults");
        return;
    }

    size_t len, expected;

    expected = sizeof(s_data.set_pressure);
    len = expected;
    if (nvs_get_blob(nvs, NVS_KEY_PRESS, &s_data.set_pressure, &len) == ESP_OK && len == expected) {
        ESP_LOGI(TAG, "NVS: loaded pressure settings");
    }

    expected = sizeof(s_data.set_doser);
    len = expected;
    if (nvs_get_blob(nvs, NVS_KEY_DOSER, &s_data.set_doser, &len) == ESP_OK && len == expected) {
        ESP_LOGI(TAG, "NVS: loaded doser settings");
    }

    expected = sizeof(s_data.set_washing);
    len = expected;
    if (nvs_get_blob(nvs, NVS_KEY_WASH, &s_data.set_washing, &len) == ESP_OK && len == expected) {
        ESP_LOGI(TAG, "NVS: loaded washing settings");
    }

    expected = sizeof(s_data.set_timeouts);
    len = expected;
    if (nvs_get_blob(nvs, NVS_KEY_TMOUT, &s_data.set_timeouts, &len) == ESP_OK && len == expected) {
        ESP_LOGI(TAG, "NVS: loaded timeout settings");
    }

    nvs_close(nvs);
}

/**
 * Инициализация хранилища данных.
 * Обнуляет структуру, устанавливает NAN для аналоговых каналов
 * (чтобы UI отображал "---" до получения первых данных),
 * заполняет уставки значениями по умолчанию, загружает сохранённые из NVS
 * и создаёт мьютекс.
 * Вызывается один раз при старте приложения до запуска задач MQTT и UI.
 */
void plant_data_init(void)
{
    memset(&s_data, 0, sizeof(s_data));
    s_data.state = PLANT_STATE_UNKNOWN;
    s_data.controller_online = false; // до прихода availability считаем offline

    // Инициализация аналоговых значений в NAN (нет данных)
    for (int i = 0; i < 4; i++) {
        s_data.pressure[i].value = NAN;
        s_data.flow[i].flow = NAN;
        s_data.flow[i].volume = NAN;
    }
    s_data.temperature.value = NAN;
    for (int i = 0; i < 4; i++) {
        s_data.conductivity[i].conductivity = NAN;
        s_data.conductivity[i].temperature = NAN;
    }
    /* KWS-306L: до прихода первого MQTT-сообщения данные неизвестны (NAN, offline) */
    for (int i = 0; i < 2; i++) {
        s_data.pumps[i].voltage     = NAN;
        s_data.pumps[i].current     = NAN;
        s_data.pumps[i].power       = NAN;
        s_data.pumps[i].energy      = NAN;
        s_data.pumps[i].temperature = NAN;
        s_data.pumps[i].online      = false;
    }

    // Уставки по умолчанию (до получения актуальных от контроллера или NVS)
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

    // Перезапись значениями из NVS (если были сохранены ранее)
    settings_load_from_nvs();

    // Создание мьютекса; configASSERT остановит систему если памяти не хватило
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex);
}

/**
 * Захват мьютекса с таймаутом (для UI-задачи).
 * @param timeout_ms максимальное время ожидания в миллисекундах
 * @return true если мьютекс успешно захвачен
 */
bool plant_data_lock(uint32_t timeout_ms)
{
    return xSemaphoreTake(s_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

/** Освобождение мьютекса (парный вызов к plant_data_lock) */
void plant_data_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

/**
 * Получение указателя на данные только для чтения.
 * ВАЖНО: вызывать только между plant_data_lock/plant_data_unlock!
 * Возвращает указатель на статическую структуру s_data.
 */
const plant_data_t *plant_data_get(void)
{
    return &s_data;
}

/**
 * Получение мутабельного указателя на данные.
 * ВАЖНО: вызывать только между plant_data_lock/plant_data_unlock!
 * Используется для прямой модификации (например, настроек из UI).
 */
plant_data_t *plant_data_get_mutable(void)
{
    return &s_data;
}

/**
 * Атомарное чтение и сброс грязных флагов.
 * ВАЖНО: вызывать только между plant_data_lock/plant_data_unlock!
 * UI-задача вызывает эту функцию, чтобы узнать какие данные обновились,
 * и затем перерисовывает только соответствующие виджеты.
 * @return битовая маска флагов, установленных с прошлого вызова
 */
uint32_t plant_data_get_and_clear_dirty(void)
{
    uint32_t flags = s_data.dirty_flags;
    s_data.dirty_flags = 0;
    return flags;
}

/* ---- Потокобезопасные сеттеры (вызываются из задачи MQTT) ---- */
/*
 * Каждый сеттер захватывает s_mutex с конечным таймаутом PLANT_DATA_LOCK_MS.
 * При таймауте — лог + drop пакета (последнее значение остаётся в s_data).
 * portMAX_DELAY вне HAL запрещён правилами проекта.
 */

/** Установить состояние автоматики, подсостояния и флаги аварий */
void plant_data_set_state(plant_state_t state, auto_sub_state_t asub,
                          wash_sub_state_t wsub, uint32_t fault_flags)
{
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_state");
        return;
    }
    s_data.state = state;
    s_data.auto_sub = asub;
    s_data.wash_sub = wsub;
    s_data.fault_flags = fault_flags;
    s_data.dirty_flags |= DIRTY_STATE;
    s_data.last_msg_time_us = esp_timer_get_time(); // метка для определения потери связи
    xSemaphoreGive(s_mutex);
}

/** Установить состояние дискретных входов и выходов */
void plant_data_set_io(uint8_t di, uint8_t do_bits)
{
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_io");
        return;
    }
    s_data.di = di;
    s_data.do_bits = do_bits;
    s_data.dirty_flags |= DIRTY_IO;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/** Установить значение датчика давления по индексу (0=P1 .. 3=P4) */
void plant_data_set_pressure(int idx, float value, bool fault)
{
    if (idx < 0 || idx >= 4) return; // защита от выхода за границы массива
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_pressure");
        return;
    }
    s_data.pressure[idx].value = value;
    s_data.pressure[idx].fault = fault;
    s_data.dirty_flags |= DIRTY_ANALOG;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/** Установить значение датчика температуры */
void plant_data_set_temperature(float value, bool fault)
{
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_temperature");
        return;
    }
    s_data.temperature.value = value;
    s_data.temperature.fault = fault;
    s_data.dirty_flags |= DIRTY_ANALOG;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/** Установить данные расходомера по индексу (0=Q1 .. 3=Q4) */
void plant_data_set_flow(int idx, float flow, float volume, bool ok)
{
    if (idx < 0 || idx >= 4) return; // защита от выхода за границы массива
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_flow");
        return;
    }
    s_data.flow[idx].flow = flow;
    s_data.flow[idx].volume = volume;
    s_data.flow[idx].ok = ok;
    s_data.dirty_flags |= DIRTY_FLOW;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/** Установить данные кондуктометра по индексу (0=s1 .. 3=s4) */
void plant_data_set_conductivity(int idx, float cond, float temp, bool ok)
{
    if (idx < 0 || idx >= 4) return; // защита от выхода за границы массива
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_conductivity");
        return;
    }
    s_data.conductivity[idx].conductivity = cond;
    s_data.conductivity[idx].temperature = temp;
    s_data.conductivity[idx].ok = ok;
    s_data.dirty_flags |= DIRTY_CONDUCTIVITY;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/** Установить снимок данных счётчика KWS-306L (idx: 0=LP, 1=HP) */
void plant_data_set_power_meter(int idx, const power_meter_data_t *data)
{
    if (idx < 0 || idx >= 2 || !data) return;
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_power_meter");
        return;
    }
    s_data.pumps[idx] = *data; // структурное копирование
    s_data.dirty_flags |= DIRTY_POWER;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/** Установить расчётную телеметрию (копирование всей структуры) */
void plant_data_set_telemetry(const telemetry_t *tel)
{
    if (!tel) return;
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_telemetry");
        return;
    }
    s_data.telemetry = *tel;
    s_data.dirty_flags |= DIRTY_TELEMETRY;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/** Установить состояние блокировок (флаги, аварийная кнопка, предупреждение фильтра) */
void plant_data_set_interlocks(uint32_t flags, bool estop, bool filter_warn)
{
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_interlocks");
        return;
    }
    s_data.interlocks.flags = flags;
    s_data.interlocks.estop = estop;
    s_data.interlocks.filter_warn = filter_warn;
    s_data.dirty_flags |= DIRTY_INTERLOCKS;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/** Установить состояние дозатора антискаланта */
void plant_data_set_doser(doser_state_t state, bool enabled)
{
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_doser");
        return;
    }
    s_data.doser.state = state;
    s_data.doser.enabled = enabled;
    s_data.dirty_flags |= DIRTY_DOSER;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/** Установить диагностику контроллера (копирование всей структуры) */
void plant_data_set_diagnostics(const diagnostics_t *diag)
{
    if (!diag) return;
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_diagnostics");
        return;
    }
    s_data.diagnostics = *diag;
    s_data.dirty_flags |= DIRTY_DIAGNOSTICS;
    s_data.last_msg_time_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}

/**
 * Установить статус MQTT-соединения.
 * Не обновляет last_msg_time_us, так как это не данные от контроллера,
 * а локальный статус сетевого подключения HMI-панели.
 */
void plant_data_set_mqtt_status(bool connected)
{
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_mqtt_status");
        return;
    }
    s_data.mqtt_connected = connected;
    s_data.dirty_flags |= DIRTY_STATE;
    xSemaphoreGive(s_mutex);
}

/**
 * Установить флаг controller_online (по топику ro_plant/availability).
 * Не обновляет last_msg_time_us — это статус контроллера, а не его данные.
 */
void plant_data_set_controller_online(bool online)
{
    if (!plant_data_try_lock()) {
        ESP_LOGW(TAG, "plant_data lock timeout in set_controller_online");
        return;
    }
    s_data.controller_online = online;
    s_data.dirty_flags |= DIRTY_CONTROLLER;
    xSemaphoreGive(s_mutex);
}

/**
 * Получить флаг controller_online (потокобезопасно).
 * При таймауте мьютекса — fallback в false (консервативно: считаем offline).
 */
bool plant_data_get_controller_online(void)
{
    bool online = false;
    if (plant_data_try_lock()) {
        online = s_data.controller_online;
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGW(TAG, "plant_data lock timeout in get_controller_online");
    }
    return online;
}

/* ---- Удобные геттеры для KWS-306L ---- */

/**
 * Внутренний помощник: вернуть копию снимка pumps[idx] под мьютексом.
 * При недоступности мьютекса возвращает структуру с .online=false.
 */
static power_meter_data_t pump_get_copy(int idx)
{
    power_meter_data_t out = {
        .voltage = NAN, .current = NAN, .power = NAN,
        .energy = NAN, .temperature = NAN, .online = false,
    };
    if (idx < 0 || idx >= 2) return out;
    /* Конечный таймаут вместо portMAX_DELAY — UI не должен блокироваться надолго */
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        out = s_data.pumps[idx];
        xSemaphoreGive(s_mutex);
    }
    return out;
}

power_meter_data_t plant_data_get_power_lp(void) { return pump_get_copy(0); }
power_meter_data_t plant_data_get_power_hp(void) { return pump_get_copy(1); }

/* ---- Сохранение уставок в NVS (вызывается из UI-задачи после "Применить") ---- */

/**
 * Вспомогательная функция: записывает blob в NVS и делает commit.
 * NVS-операции выполняются ВНЕ мьютекса, т.к. flash-запись может занять время.
 */
static void nvs_save_blob(const char *key, const void *data, size_t len)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_blob(nvs, key, data, len);
        nvs_commit(nvs);
        nvs_close(nvs);
    } else {
        ESP_LOGE(TAG, "NVS: failed to open for writing");
    }
}

void plant_data_save_settings_pressure(const settings_pressure_t *s)
{
    if (!s) return;
    if (plant_data_try_lock()) {
        s_data.set_pressure = *s;
        s_data.dirty_flags |= DIRTY_SETTINGS;
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGW(TAG, "plant_data lock timeout in save_settings_pressure");
        /* Кэш не обновлён, но запись в NVS всё равно делаем — следующий старт подхватит */
    }
    nvs_save_blob(NVS_KEY_PRESS, s, sizeof(*s));
    ESP_LOGI(TAG, "NVS: saved pressure settings");
}

void plant_data_save_settings_doser(const settings_doser_t *s)
{
    if (!s) return;
    if (plant_data_try_lock()) {
        s_data.set_doser = *s;
        s_data.dirty_flags |= DIRTY_SETTINGS;
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGW(TAG, "plant_data lock timeout in save_settings_doser");
    }
    nvs_save_blob(NVS_KEY_DOSER, s, sizeof(*s));
    ESP_LOGI(TAG, "NVS: saved doser settings");
}

void plant_data_save_settings_washing(const settings_washing_t *s)
{
    if (!s) return;
    if (plant_data_try_lock()) {
        s_data.set_washing = *s;
        s_data.dirty_flags |= DIRTY_SETTINGS;
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGW(TAG, "plant_data lock timeout in save_settings_washing");
    }
    nvs_save_blob(NVS_KEY_WASH, s, sizeof(*s));
    ESP_LOGI(TAG, "NVS: saved washing settings");
}

void plant_data_save_settings_timeouts(const settings_timeouts_t *s)
{
    if (!s) return;
    if (plant_data_try_lock()) {
        s_data.set_timeouts = *s;
        s_data.dirty_flags |= DIRTY_SETTINGS;
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGW(TAG, "plant_data lock timeout in save_settings_timeouts");
    }
    nvs_save_blob(NVS_KEY_TMOUT, s, sizeof(*s));
    ESP_LOGI(TAG, "NVS: saved timeout settings");
}
#endif /* !LVGL_LIVE_PREVIEW */
