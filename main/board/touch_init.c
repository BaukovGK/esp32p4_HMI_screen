#include "touch_init.h"
#include "board.h"
#include "esp_lcd_touch_gsl3680.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "touch";

static i2c_master_bus_handle_t s_i2c_bus = NULL;

static esp_err_t i2c_init(void)
{
    if (s_i2c_bus) {
        return ESP_OK;
    }

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
    return i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
}

lv_indev_t *touch_init(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Initializing touch controller");

    /* 1. Initialize I2C bus */
    ESP_ERROR_CHECK(i2c_init());

    /* 2. Create I2C panel IO for touch */
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GSL3680_CONFIG();
    tp_io_cfg.scl_speed_hz = BOARD_TOUCH_I2C_FREQ_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_cfg, &tp_io));

    /* 3. Configure touch controller */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .rst_gpio_num = BOARD_TOUCH_RST_GPIO,
        .int_gpio_num = BOARD_TOUCH_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };

    /* 4. Create GSL3680 touch handle */
    esp_lcd_touch_handle_t touch = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gsl3680(tp_io, &tp_cfg, &touch));
    ESP_LOGI(TAG, "GSL3680 touch controller initialized");

    /* 5. Register touch device with LVGL */
    const lvgl_port_touch_cfg_t touch_lvgl_cfg = {
        .disp = disp,
        .handle = touch,
    };

    lv_indev_t *indev = lvgl_port_add_touch(&touch_lvgl_cfg);
    if (!indev) {
        ESP_LOGE(TAG, "Failed to add touch to LVGL");
        return NULL;
    }

    ESP_LOGI(TAG, "Touch input registered with LVGL");
    return indev;
}
