cmake_minimum_required(VERSION 3.16)

# Жёстко закрепляем target — проект только под Waveshare ESP32-P4-NANO.
# Это нужно для активации CONFIG_SOC_MIPI_DSI_SUPPORTED (DSI-источники в
# esp_lcd собираются под `if(CONFIG_SOC_MIPI_DSI_SUPPORTED)`). Без этого
# `esp_lcd_mipi_dsi.h` не находится при сборке display_init.c.
# Если кто-то всё-таки запустит `idf.py set-target xxx` для экспериментов,
# `set()` здесь перебивается командой; для возврата — стереть build/.
set(IDF_TARGET "esp32p4")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ro_hmi)
