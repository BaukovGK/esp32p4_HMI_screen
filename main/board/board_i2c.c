/**
 * @file board_i2c.c
 * @brief Инициализация общей шины I2C (master) для периферии платы.
 *
 * Выделяет инициализацию I2C из touch_init.c в отдельный модуль,
 * чтобы и display_init (предварительная инициализация MCU Waveshare)
 * и touch_init (тач-контроллер GT911) использовали одну шину.
 */
#ifndef LVGL_LIVE_PREVIEW
#include "board_i2c.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include <stdio.h>

static const char *TAG = "board_i2c";

static i2c_master_bus_handle_t s_i2c_bus = NULL;
/** Защита s_i2c_bus от гонок init↔get; функции i2c_master_* сами потокобезопасны
 *  и не требуют дополнительной обвязки, но публичный геттер требует барьера. */
static portMUX_TYPE s_i2c_bus_mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * Сканирование шины I2C — поиск всех отвечающих устройств.
 * Выводит таблицу в лог (как i2cdetect в Linux).
 */
static void board_i2c_scan(void)
{
    ESP_LOGI(TAG, "I2C bus scan (SDA=%d, SCL=%d):",
             BOARD_TOUCH_I2C_SDA, BOARD_TOUCH_I2C_SCL);
    ESP_LOGI(TAG, "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");

    int found = 0;
    for (uint8_t row = 0; row < 8; row++) {
        char line[64];
        int pos = snprintf(line, sizeof(line), "%02x: ", row << 4);
        for (uint8_t col = 0; col < 16; col++) {
            uint8_t addr = (row << 4) | col;
            if (addr < 0x03 || addr > 0x77) {
                pos += snprintf(line + pos, sizeof(line) - pos, "   ");
                continue;
            }
            esp_err_t ret = i2c_master_probe(s_i2c_bus, addr, 50);
            if (ret == ESP_OK) {
                pos += snprintf(line + pos, sizeof(line) - pos, "%02x ", addr);
                found++;
            } else {
                pos += snprintf(line + pos, sizeof(line) - pos, "-- ");
            }
        }
        ESP_LOGI(TAG, "%s", line);
    }
    ESP_LOGI(TAG, "I2C scan done: %d device(s) found", found);
}

esp_err_t board_i2c_init(void)
{
    portENTER_CRITICAL(&s_i2c_bus_mux);
    if (s_i2c_bus) {
        portEXIT_CRITICAL(&s_i2c_bus_mux);
        return ESP_OK;
    }
    portEXIT_CRITICAL(&s_i2c_bus_mux);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BOARD_TOUCH_I2C_PORT,
        .sda_io_num = BOARD_TOUCH_I2C_SDA,
        .scl_io_num = BOARD_TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    i2c_master_bus_handle_t tmp_bus = NULL;
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &tmp_bus);
    if (ret == ESP_OK) {
        portENTER_CRITICAL(&s_i2c_bus_mux);
        s_i2c_bus = tmp_bus;
        portEXIT_CRITICAL(&s_i2c_bus_mux);
        ESP_LOGI(TAG, "I2C bus initialized (SDA=%d, SCL=%d)",
                 BOARD_TOUCH_I2C_SDA, BOARD_TOUCH_I2C_SCL);
        board_i2c_scan();
    }
    return ret;
}

i2c_master_bus_handle_t board_i2c_get_bus(void)
{
    portENTER_CRITICAL(&s_i2c_bus_mux);
    i2c_master_bus_handle_t handle = s_i2c_bus;
    portEXIT_CRITICAL(&s_i2c_bus_mux);
    return handle;
}
#endif /* !LVGL_LIVE_PREVIEW */
