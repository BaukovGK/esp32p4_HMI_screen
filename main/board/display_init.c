#ifndef LVGL_LIVE_PREVIEW
/**
 * @file display_init.c
 * @brief Инициализация дисплея 10.1" (MIPI DSI, контроллер JD9365) и регистрация в LVGL.
 *
 * Модуль выполняет полную настройку дисплейной подсистемы:
 * - ШИМ-подсветка через LEDC (GPIO23, 20 кГц, 10-бит)
 * - Питание PHY MIPI DSI через внутренний LDO (канал 3, 2.5 В)
 * - Создание шины MIPI DSI (2 линии данных, 1 Гбит/с на линию)
 * - DBI-интерфейс для передачи команд инициализации панели
 * - DPI-интерфейс для передачи видеопотока (800x1280, RGB565, 60 Гц)
 * - Регистрация дисплея в LVGL с программным поворотом на 270 градусов (ландшафт 1280x800)
 *
 * Зависимости: board.h (аппаратные константы), esp_lcd_jd9365 (драйвер панели),
 *              esp_lvgl_port (интеграция LVGL с ESP-IDF)
 */
#include "display_init.h"
#include "board.h"
#include "esp_lcd_jd9365.h"       // Драйвер LCD-контроллера JD9365
#include "esp_lcd_panel_ops.h"    // Общие операции с LCD-панелями (reset, init, on/off)
#include "esp_lcd_mipi_dsi.h"     // API шины MIPI DSI
#include "esp_ldo_regulator.h"    // Управление встроенными LDO-регуляторами ESP32-P4
#include "esp_lvgl_port.h"        // Интеграция LVGL с ESP-IDF (порт)
#include "driver/ledc.h"          // ШИМ-контроллер LEDC для подсветки
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "display"; // Тег для логирования

#define LCD_LEDC_CH  0 // Канал LEDC для ШИМ-подсветки дисплея

/**
 * Инициализация ШИМ-подсветки дисплея через LEDC.
 *
 * Настраивает таймер LEDC (20 кГц, 10-бит разрешение) и канал,
 * привязанный к GPIO подсветки. После инициализации подсветка выключена (duty=0).
 *
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
static esp_err_t backlight_init(void)
{
    // Настройка таймера LEDC: низкоскоростной режим, 20 кГц, 10-бит разрешение
    const ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT, // 0..1023 уровней яркости
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 20000, // 20 кГц — выше слышимого диапазона, без мерцания
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "LEDC timer config failed");

    // Настройка канала LEDC: привязка к GPIO подсветки, начальная скважность 0 (выкл.)
    const ledc_channel_config_t ch_cfg = {
        .gpio_num = BOARD_LCD_BACKLIGHT_GPIO, // GPIO23
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0, // Подсветка выключена при инициализации
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "LEDC channel config failed");
    return ESP_OK;
}

/**
 * Установка яркости подсветки дисплея.
 *
 * @param brightness_percent Яркость в процентах (0..100). Значения за пределами диапазона
 *                           автоматически ограничиваются (clamp).
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
static esp_err_t backlight_set(int brightness_percent)
{
    // Ограничение значения яркости в допустимом диапазоне
    if (brightness_percent < 0) brightness_percent = 0;
    if (brightness_percent > 100) brightness_percent = 100;
    // Пересчёт процентов в значение скважности (10-бит: 0..1023)
    uint32_t duty = (1023 * brightness_percent) / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty), TAG, "Set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH), TAG, "Update duty failed");
    return ESP_OK;
}

/**
 * Полная инициализация дисплея и регистрация в LVGL.
 *
 * Последовательность:
 * 1. Инициализация ШИМ-подсветки (выключена до конца инициализации)
 * 2. Включение питания PHY MIPI DSI через внутренний LDO
 * 3. Создание шины MIPI DSI (2 линии данных)
 * 4. Создание DBI-интерфейса для команд инициализации панели
 * 5. Настройка DPI-тайминга (800x1280, 60 Гц, RGB565)
 * 6. Создание и инициализация панели JD9365
 * 7. Инициализация LVGL-порта
 * 8. Регистрация дисплея в LVGL (буферы в SPIRAM + DMA)
 * 9. Программный поворот на 270 градусов (ландшафтный режим 1280x800)
 * 10. Включение подсветки на 100%
 *
 * @return Указатель на LVGL-дисплей, или NULL при ошибке
 */
lv_display_t *display_init(void)
{
    ESP_LOGI(TAG, "Initializing display");

    /* 1. Инициализация ШИМ-подсветки (подсветка пока выключена) */
    ESP_ERROR_CHECK(backlight_init());

    /* 2. Включение питания PHY MIPI DSI через встроенный LDO ESP32-P4 */
#if BOARD_MIPI_DSI_PHY_LDO_CHAN > 0
    esp_ldo_channel_handle_t ldo_mipi = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BOARD_MIPI_DSI_PHY_LDO_CHAN, // Канал 3
        .voltage_mv = BOARD_MIPI_DSI_PHY_LDO_MV, // 2500 мВ
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &ldo_mipi));
    ESP_LOGI(TAG, "MIPI DSI PHY powered on");
#endif

    /* 3. Создание шины MIPI DSI (2 линии данных, конфигурация из макроса JD9365) */
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_cfg = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus));
    ESP_LOGI(TAG, "MIPI DSI bus created");

    /* 4. Создание DBI panel I/O (командный интерфейс для отправки init-последовательности) */
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_dbi_io_config_t dbi_cfg = JD9365_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, &io));

    /* 5. Настройка DPI-тайминга видеопотока (800x1280, 60 Гц, формат RGB565) */
    esp_lcd_dpi_panel_config_t dpi_cfg = JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);

    /* 6. Создание панели JD9365 с настройками из board.h */
    jd9365_vendor_config_t vendor_cfg = {
        .init_cmds = NULL,      // Используется встроенная init-последовательность драйвера
        .init_cmds_size = 0,
        .mipi_config = {
            .dsi_bus = dsi_bus,
            .dpi_config = &dpi_cfg,
            .lane_num = BOARD_LCD_MIPI_DSI_LANES, // 2 линии данных
        },
    };

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_LCD_RST_GPIO,   // GPIO27 — аппаратный сброс панели
        .rgb_ele_order = BOARD_LCD_COLOR_SPACE,  // RGB
        .bits_per_pixel = BOARD_LCD_BPP,         // 16 бит (RGB565)
        .vendor_config = &vendor_cfg,
    };

    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9365(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));  // Аппаратный сброс панели
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));   // Отправка init-последовательности
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true)); // Включение отображения
    ESP_LOGI(TAG, "LCD panel initialized");

    /* 7. Инициализация LVGL-порта (создание задачи LVGL, мьютекса и т.д.) */
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    /* 8. Регистрация дисплея в LVGL (буферы размещаются в SPIRAM с DMA-доступом) */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .control_handle = NULL,
        .buffer_size = BOARD_LCD_DRAW_BUFF_SIZE, // 800 * 80 = 64000 пикселей
        .double_buffer = false,                   // Одинарный буфер (экономия памяти)
        .hres = BOARD_LCD_H_RES,                 // 800 (нативное)
        .vres = BOARD_LCD_V_RES,                 // 1280 (нативное)
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,  // Формат цвета для LVGL 9+
#endif
        .flags = {
            .buff_dma = true,       // Буферы доступны для DMA
            .buff_spiram = true,    // Буферы размещены в SPIRAM (PSRAM)
            .sw_rotate = true,      // Включить программный поворот
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = (BOARD_LCD_BIGENDIAN ? true : false), // Перестановка байт для big-endian
#endif
        },
    };

    // Конфигурация DSI-специфичных параметров дисплея
    const lvgl_port_display_dsi_cfg_t dsi_disp_cfg = {
        .flags = {
            .avoid_tearing = false, // Защита от тиринга отключена
        },
    };

    lv_display_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to add display to LVGL");
        return NULL;
    }

    /* 9. Программный поворот на 270 градусов — ландшафтный режим (1280x800) */
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    ESP_LOGI(TAG, "Display ready (landscape 1280x800, sw rotation 270)");

    /* 10. Включение подсветки на 100% */
    ESP_ERROR_CHECK(backlight_set(100));
    ESP_LOGI(TAG, "Backlight on");

    return disp;
}
#endif /* !LVGL_LIVE_PREVIEW */
