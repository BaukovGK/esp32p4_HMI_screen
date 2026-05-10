#ifndef LVGL_LIVE_PREVIEW
/**
 * @file display_init.c
 * @brief Инициализация дисплея 10.1" (MIPI DSI, контроллер JD9365) и регистрация в LVGL.
 *
 * Модуль выполняет полную настройку дисплейной подсистемы:
 * - ШИМ-подсветка через LEDC (GPIO23, 20 кГц, 10-бит)
 * - Питание PHY MIPI DSI через внутренний LDO (канал 3, 2.5 В)
 * - Создание шины MIPI DSI (2 линии данных, 1.5 Гбит/с на линию)
 * - DBI-интерфейс для передачи команд инициализации панели
 * - DPI-интерфейс для передачи видеопотока (800x1280, RGB565, 60 Гц)
 * - Регистрация дисплея в LVGL с программным поворотом на 270 градусов (ландшафт 1280x800)
 *
 * На плате Waveshare ESP32-P4-NANO дисплей управляется аппаратным сбросом через GPIO 27.
 * MCU дисплея (I2C 0x45) инициализируется внутри компонента waveshare/esp_lcd_jd9365_10_1.
 *
 * Зависимости: board.h (аппаратные константы),
 *              waveshare/esp_lcd_jd9365_10_1 (драйвер панели),
 *              esp_lvgl_port (интеграция LVGL с ESP-IDF)
 */
#include "display_init.h"
#include "board.h"
#include "esp_lcd_jd9365_10_1.h"   // Waveshare-специфичный драйвер JD9365 для 10.1"
#include "esp_lcd_panel_ops.h"     // Общие операции с LCD-панелями (reset, init, on/off)
#include "esp_lcd_mipi_dsi.h"      // API шины MIPI DSI
#include "esp_ldo_regulator.h"     // Управление встроенными LDO-регуляторами ESP32-P4
#include "esp_lvgl_port.h"         // Интеграция LVGL с ESP-IDF (порт)
#include "driver/ledc.h"           // ШИМ-контроллер LEDC для подсветки
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "display";

/* Хэндлы дисплейной подсистемы вынесены в module-static, чтобы при ошибке
 * на любом шаге cleanup-метка могла корректно освободить уже захваченные ресурсы,
 * а display_deinit() мог выполнить тот же teardown по запросу. */
static esp_ldo_channel_handle_t s_ldo_mipi = NULL;
static esp_lcd_dsi_bus_handle_t s_dsi_bus = NULL;
static esp_lcd_panel_io_handle_t s_io = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;
static bool s_lvgl_port_inited = false;

/**
 * Инициализация ШИМ-подсветки дисплея через LEDC.
 */
static esp_err_t backlight_init(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BOARD_LCD_LEDC_DUTY_BITS,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = BOARD_LCD_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "LEDC timer config failed");

    const ledc_channel_config_t ch_cfg = {
        .gpio_num = BOARD_LCD_BACKLIGHT_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BOARD_LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "LEDC channel config failed");
    return ESP_OK;
}

/**
 * Установка яркости подсветки дисплея.
 */
static esp_err_t backlight_set(int brightness_percent)
{
    if (brightness_percent < 0) brightness_percent = 0;
    if (brightness_percent > 100) brightness_percent = 100;
    uint32_t duty = (1023 * brightness_percent) / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, BOARD_LCD_LEDC_CH, duty), TAG, "Set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, BOARD_LCD_LEDC_CH), TAG, "Update duty failed");
    return ESP_OK;
}

/**
 * Полная инициализация дисплея и регистрация в LVGL.
 *
 * Аппаратный сброс дисплея через GPIO 27 выполняется внутри esp_lcd_panel_reset().
 *
 * @return Указатель на LVGL-дисплей, или NULL при ошибке
 */
lv_display_t *display_init(void)
{
    ESP_LOGI(TAG, "Initializing display");
    esp_err_t err;

    /* 1. Инициализация ШИМ-подсветки (подсветка пока выключена).
     *    LEDC-операции редко падают, но проверяем — единый стиль обработки ошибок. */
    err = backlight_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "backlight_init failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    /* 2. Включение питания PHY MIPI DSI через встроенный LDO ESP32-P4 */
#if BOARD_MIPI_DSI_PHY_LDO_CHAN > 0
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BOARD_MIPI_DSI_PHY_LDO_CHAN,
        .voltage_mv = BOARD_MIPI_DSI_PHY_LDO_MV,
    };
    err = esp_ldo_acquire_channel(&ldo_cfg, &s_ldo_mipi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ldo_acquire_channel failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    ESP_LOGI(TAG, "MIPI DSI PHY powered on");
#endif

    /* 3. Создание шины MIPI DSI (2 линии данных) */
    esp_lcd_dsi_bus_config_t bus_cfg = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
    err = esp_lcd_new_dsi_bus(&bus_cfg, &s_dsi_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_dsi_bus failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    ESP_LOGI(TAG, "MIPI DSI bus created");

    /* 4. Создание DBI panel I/O (командный интерфейс) */
    esp_lcd_dbi_io_config_t dbi_cfg = JD9365_PANEL_IO_DBI_CONFIG();
    err = esp_lcd_new_panel_io_dbi(s_dsi_bus, &dbi_cfg, &s_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_dbi failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    /* 5. Настройка DPI-тайминга видеопотока (800x1280, 60 Гц, RGB565) */
    esp_lcd_dpi_panel_config_t dpi_cfg = JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    dpi_cfg.num_fbs = 2; // Макрос ставит 1; double_buffer для плавной отрисовки

    /* 6. Создание панели JD9365 (Waveshare 10.1")
     *    I2C MCU pre-init (0x45) выполняется автоматически внутри esp_lcd_new_panel_jd9365() */
    jd9365_vendor_config_t vendor_cfg = {
        .flags = {
            .use_mipi_interface = 1,
        },
        .mipi_config = {
            .dsi_bus = s_dsi_bus,
            .dpi_config = &dpi_cfg,
            .lane_num = BOARD_LCD_MIPI_DSI_LANES,
        },
    };

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_LCD_RST_GPIO,
        .rgb_ele_order = BOARD_LCD_COLOR_SPACE,
        .bits_per_pixel = BOARD_LCD_BPP,
        .vendor_config = &vendor_cfg,
    };

    err = esp_lcd_new_panel_jd9365(s_io, &panel_cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_jd9365 failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    err = esp_lcd_panel_reset(s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_panel_reset failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    err = esp_lcd_panel_init(s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_panel_init failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    err = esp_lcd_panel_disp_on_off(s_panel, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_panel_disp_on_off failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    ESP_LOGI(TAG, "LCD panel initialized");

    /* 7. Инициализация LVGL-порта */
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    err = lvgl_port_init(&lvgl_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    s_lvgl_port_inited = true;

    /* 8. Регистрация дисплея в LVGL (буферы в SPIRAM + DMA) */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_io,
        .panel_handle = s_panel,
        .control_handle = NULL,
        .buffer_size = BOARD_LCD_DRAW_BUFF_SIZE,
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .sw_rotate = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = (BOARD_LCD_BIGENDIAN ? true : false),
#endif
        },
    };

    /* avoid_tearing = false: sw_rotate несовместим с avoid_tearing в esp_lvgl_port.
     * При avoid_tearing LVGL рисует напрямую в DSI frame buffer, но буфер
     * программного поворота (draw_buffs[2]) не связан с DSI — результат: белый экран.
     * Без avoid_tearing LVGL использует свои маленькие draw-буферы + draw_bitmap. */
    const lvgl_port_display_dsi_cfg_t dsi_disp_cfg = {
        .flags = {
            .avoid_tearing = false,
        },
    };

    lv_display_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to add display to LVGL");
        goto cleanup;
    }

    /* 9. Программный поворот на 270 градусов — ландшафтный режим (1280x800) */
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    ESP_LOGI(TAG, "Display ready (landscape 1280x800, sw rotation 270)");

    /* 10. Включение подсветки на 100%.
     *     Если LEDC update вдруг упадёт — экран покажется чёрным, но это не повод
     *     рушить всю инициализацию: дисплей уже работает. Логируем и продолжаем. */
    err = backlight_set(100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "backlight_set failed: %s (display will be dark)", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Backlight on");
    }

    return disp;

cleanup:
    /* Освобождаем ресурсы в порядке, обратном захвату.
     * Backlight (LEDC) намеренно не останавливаем — он на отдельном GPIO,
     * не привязан к DSI и его «зависшее» состояние безопасно. */
    if (s_lvgl_port_inited) {
        lvgl_port_deinit();
        s_lvgl_port_inited = false;
    }
    if (s_panel) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
    }
    if (s_io) {
        esp_lcd_panel_io_del(s_io);
        s_io = NULL;
    }
    if (s_dsi_bus) {
        esp_lcd_del_dsi_bus(s_dsi_bus);
        s_dsi_bus = NULL;
    }
    if (s_ldo_mipi) {
        esp_ldo_release_channel(s_ldo_mipi);
        s_ldo_mipi = NULL;
    }
    return NULL;
}

/**
 * Деинициализация дисплейной подсистемы.
 *
 * Освобождает все ресурсы, захваченные display_init() (LDO канал, DSI шину,
 * panel I/O, panel handle, LVGL port). Backlight остаётся в текущем состоянии —
 * LEDC канал не освобождается намеренно (отдельный GPIO, не связан с DSI).
 *
 * Безопасно вызывать повторно или после неудачного display_init().
 */
void display_deinit(void)
{
    if (s_lvgl_port_inited) {
        lvgl_port_deinit();
        s_lvgl_port_inited = false;
    }
    if (s_panel) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
    }
    if (s_io) {
        esp_lcd_panel_io_del(s_io);
        s_io = NULL;
    }
    if (s_dsi_bus) {
        esp_lcd_del_dsi_bus(s_dsi_bus);
        s_dsi_bus = NULL;
    }
    if (s_ldo_mipi) {
        esp_ldo_release_channel(s_ldo_mipi);
        s_ldo_mipi = NULL;
    }
}
#endif /* !LVGL_LIVE_PREVIEW */
