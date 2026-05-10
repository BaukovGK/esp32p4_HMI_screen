/**
 * @file board.h
 * @brief Аппаратные определения платы HMI-дисплея установки обратного осмоса.
 *
 * Содержит все аппаратные константы: назначение GPIO-пинов, параметры
 * MIPI DSI дисплея (JD9365), сенсорного контроллера (GT911, I2C),
 * питание PHY через LDO и настройки буфера отрисовки LVGL.
 *
 * Платформа: Waveshare ESP32-P4-NANO, дисплей 10.1" 800x1280 (портрет),
 * поворот в 1280x800 (ландшафт).
 */
#pragma once

#ifndef LVGL_LIVE_PREVIEW
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "soc/soc_caps.h"
#else
/* Заглушки GPIO для режима предпросмотра LVGL (без реального железа) */
typedef int gpio_num_t;
#define GPIO_NUM_7   7
#define GPIO_NUM_8   8
#define GPIO_NUM_21  21
#define GPIO_NUM_22  22
#define GPIO_NUM_23  23
#define GPIO_NUM_27  27
#define I2C_NUM_1    1
#define LEDC_TIMER_10_BIT  10
#endif

/* ---- Дисплей MIPI DSI (контроллер JD9365, нативное разрешение 800x1280 портрет) ---- */
#define BOARD_LCD_H_RES             800     // Горизонтальное разрешение (нативное, до поворота)
#define BOARD_LCD_V_RES             1280    // Вертикальное разрешение (нативное, до поворота)
#define BOARD_LCD_MIPI_DSI_LANES    2       // Количество линий данных MIPI DSI
#define BOARD_LCD_COLOR_SPACE       ESP_LCD_COLOR_SPACE_RGB // Цветовое пространство RGB
#define BOARD_LCD_BPP               16      // Глубина цвета: RGB565 (16 бит на пиксель)
#define BOARD_LCD_BIGENDIAN         0       // Порядок байт: little-endian
#define BOARD_LCD_BACKLIGHT_GPIO    GPIO_NUM_23 // GPIO управления подсветкой (ШИМ через LEDC)
#define BOARD_LCD_RST_GPIO          GPIO_NUM_27 // Аппаратный сброс LCD (GPIO27)

/* ---- MCU дисплея Waveshare (I2C 0x45) ---- */
/* MCU дисплея присутствует на шине I2C (адрес 0x45).
 * Предварительная инициализация (reg 0x95, 0x96) выполняется внутри
 * компонента waveshare/esp_lcd_jd9365_10_1 автоматически.
 * Аппаратный сброс LCD через GPIO 27. */

/* ---- Питание PHY MIPI DSI через встроенный LDO ---- */
#define BOARD_MIPI_DSI_PHY_LDO_CHAN 3       // Канал внутреннего LDO-регулятора ESP32-P4
#define BOARD_MIPI_DSI_PHY_LDO_MV  2500    // Напряжение питания PHY: 2.5 В

/* ---- Сенсорная панель (контроллер GT911, интерфейс I2C) ---- */
#define BOARD_TOUCH_I2C_PORT        I2C_NUM_1   // Порт I2C (I2C1 — как в Waveshare BSP)
#define BOARD_TOUCH_I2C_SDA         GPIO_NUM_7  // GPIO линии данных I2C SDA
#define BOARD_TOUCH_I2C_SCL         GPIO_NUM_8  // GPIO линии тактирования I2C SCL
#define BOARD_TOUCH_I2C_FREQ_HZ     400000      // Частота I2C: 400 кГц (Fast Mode)
#define BOARD_TOUCH_RST_GPIO        GPIO_NUM_22 // GPIO аппаратного сброса тач-контроллера
#define BOARD_TOUCH_INT_GPIO        GPIO_NUM_21 // GPIO прерывания от тач-контроллера

/* ---- Ландшафтный режим (после программного поворота на 270 градусов) ---- */
#define BOARD_DISP_H_RES            1280    // Горизонтальное разрешение в ландшафтном режиме
#define BOARD_DISP_V_RES            800     // Вертикальное разрешение в ландшафтном режиме

/* ---- ШИМ-подсветка (LEDC) ---- */
#define BOARD_LCD_LEDC_CH        0
#define BOARD_LCD_LEDC_FREQ_HZ   20000
#define BOARD_LCD_LEDC_DUTY_BITS LEDC_TIMER_10_BIT
#define BOARD_LCD_LEDC_MAX_DUTY  ((1U << BOARD_LCD_LEDC_DUTY_BITS) - 1) // 10-bit: 1023

/* ---- Буфер отрисовки LVGL ----
 *
 * При sw_rotate (поворот 270° → ландшафт 1280x800) буфер интерпретируется в
 * native-portrait геометрии (800 px × N строк), а LVGL выполняет несколько
 * partial redraws на full-screen операцию. Размер выбран как baseline:
 * 800×80 ≈ 64K px ≈ 128 KB на буфер; double-buffer ≈ 256 KB в PSRAM.
 * При жалобах на медленную перерисовку (или tearing/scrolling в ландшафте)
 * — увеличить BOARD_LCD_DRAW_BUFF_LINES до 120 или 160 (ограничение — память PSRAM).
 * Менять без профилирования на железе не следует. */
#define BOARD_LCD_DRAW_BUFF_LINES   80      // Количество строк в буфере отрисовки
#define BOARD_LCD_DRAW_BUFF_SIZE    (BOARD_LCD_H_RES * BOARD_LCD_DRAW_BUFF_LINES) // Размер буфера в пикселях (800 * 80 = 64000)

/* Sanity-check: буфер должен покрывать как минимум одну полную строку дисплея,
 * иначе LVGL flush станет крайне неэффективным. */
_Static_assert(BOARD_LCD_DRAW_BUFF_SIZE >= BOARD_LCD_H_RES,
               "draw buffer должен быть >= ширины дисплея для эффективной отрисовки");
