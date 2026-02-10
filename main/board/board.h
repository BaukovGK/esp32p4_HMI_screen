#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "soc/soc_caps.h"

/* ---- MIPI DSI Display (JD9365, 800x1280 native) ---- */
#define BOARD_LCD_H_RES             800
#define BOARD_LCD_V_RES             1280
#define BOARD_LCD_MIPI_DSI_LANES    2
#define BOARD_LCD_MIPI_LANE_BITRATE 1000    /* Mbps */
#define BOARD_LCD_COLOR_SPACE       ESP_LCD_COLOR_SPACE_RGB
#define BOARD_LCD_BPP               16      /* RGB565 */
#define BOARD_LCD_BIGENDIAN         0
#define BOARD_LCD_BACKLIGHT_GPIO    GPIO_NUM_23
#define BOARD_LCD_RST_GPIO          GPIO_NUM_27

/* MIPI DSI PHY LDO power */
#define BOARD_MIPI_DSI_PHY_LDO_CHAN 3
#define BOARD_MIPI_DSI_PHY_LDO_MV  2500

/* ---- Touch (GSL3680, I2C) ---- */
#define BOARD_TOUCH_I2C_PORT        I2C_NUM_0
#define BOARD_TOUCH_I2C_SDA         GPIO_NUM_7
#define BOARD_TOUCH_I2C_SCL         GPIO_NUM_8
#define BOARD_TOUCH_I2C_FREQ_HZ     400000
#define BOARD_TOUCH_RST_GPIO        GPIO_NUM_22
#define BOARD_TOUCH_INT_GPIO        GPIO_NUM_21

/* ---- Landscape (after software rotation) ---- */
#define BOARD_DISP_H_RES            1280
#define BOARD_DISP_V_RES            800

/* ---- LVGL Draw Buffer ---- */
#define BOARD_LCD_DRAW_BUFF_LINES   80
#define BOARD_LCD_DRAW_BUFF_SIZE    (BOARD_LCD_H_RES * BOARD_LCD_DRAW_BUFF_LINES)
