/**
 * @file sim/lv_conf.h
 * @brief Конфигурация LVGL для нативного SDL2-симулятора (macOS/Linux).
 *
 * Используется ТОЛЬКО при сборке из sim/CMakeLists.txt. ESP-IDF
 * собирает LVGL по своему Kconfig (см. sdkconfig). Версия LVGL — 9.2.2.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ════════════════════════════════════════════════════════════════════
 * COLOR / DISPLAY
 * ════════════════════════════════════════════════════════════════════ */

/* Глубина цвета — 32 (RGBA) для современных macOS дисплеев. На ESP32-P4
 * стоит 16 (RGB565), но в симуляторе можно использовать полные цвета. */
#define LV_COLOR_DEPTH      32
#define LV_COLOR_16_SWAP    0

/* ════════════════════════════════════════════════════════════════════
 * MEMORY
 * ════════════════════════════════════════════════════════════════════ */

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_MEM_SIZE             (4 * 1024 * 1024)   /* 4 MB на симуляторе */
#define LV_MEM_POOL_INCLUDE     <stdlib.h>

#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

/* ════════════════════════════════════════════════════════════════════
 * TIMER / TICK
 * ════════════════════════════════════════════════════════════════════ */

/* В SDL-симуляторе тикaeт через lv_tick_inc() из основного цикла либо
 * автоматически через SDL_GetTicks (CUSTOM_TICK_GET). */
#define LV_TICK_CUSTOM              1
#define LV_TICK_CUSTOM_INCLUDE      "SDL.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (SDL_GetTicks())

/* ════════════════════════════════════════════════════════════════════
 * RENDERING
 * ════════════════════════════════════════════════════════════════════ */

#define LV_USE_DRAW_SW            1
#define LV_DRAW_SW_SUPPORT_RGB565 1
#define LV_DRAW_SW_SUPPORT_RGB888 1
#define LV_DRAW_SW_SUPPORT_ARGB8888 1

/* Никакого hw-acceleration в симуляторе. */
#define LV_USE_DRAW_VG_LITE  0
#define LV_USE_DRAW_PXP      0

/* ════════════════════════════════════════════════════════════════════
 * FONTS
 * ════════════════════════════════════════════════════════════════════ */

#define LV_FONT_MONTSERRAT_14    1   /* default font */

/* Все остальные используем кастомные из main/fonts/Montserrat_*_4.c.
 * Они подключаются в C-коде через ui_fonts.h. */

#define LV_FONT_DEFAULT          &lv_font_montserrat_14

/* ════════════════════════════════════════════════════════════════════
 * WIDGETS — включаем все, симулятор может позволить себе
 * ════════════════════════════════════════════════════════════════════ */

#define LV_USE_OBJ          1
#define LV_USE_LABEL        1
#define LV_USE_LINE         1
#define LV_USE_BUTTON       1
#define LV_USE_IMAGE        1
#define LV_USE_BAR          1
#define LV_USE_ARC          1
#define LV_USE_SLIDER       1
#define LV_USE_TABLE        1
#define LV_USE_TEXTAREA     1
#define LV_USE_DROPDOWN     1
#define LV_USE_CHECKBOX     1
#define LV_USE_SWITCH       1
#define LV_USE_KEYBOARD     1
#define LV_USE_LIST         1
#define LV_USE_MENU         1
#define LV_USE_MSGBOX       1
#define LV_USE_ROLLER       1
#define LV_USE_SPAN         1
#define LV_USE_SPINBOX      1
#define LV_USE_SPINNER      1
#define LV_USE_TABVIEW      1
#define LV_USE_TILEVIEW     1
#define LV_USE_WIN          1
#define LV_USE_CHART        1
#define LV_USE_CALENDAR     0
#define LV_USE_COLORWHEEL   0
#define LV_USE_LED          1

/* Theme. */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_BASIC   1
#define LV_USE_THEME_MONO    0

/* ════════════════════════════════════════════════════════════════════
 * SDL DRIVER (КЛЮЧЕВОЕ для симулятора)
 * ════════════════════════════════════════════════════════════════════ */

#define LV_USE_SDL              1
#if LV_USE_SDL
    /* Important: используем "SDL.h" (без SDL2/ префикса), потому что
     * sdl2-config --cflags даёт `-I/opt/homebrew/include/SDL2` —
     * include path уже указывает В директорию SDL2, не НА неё. */
    #define LV_SDL_INCLUDE_PATH     "SDL.h"
    #define LV_SDL_RENDER_MODE      LV_DISPLAY_RENDER_MODE_DIRECT
    #define LV_SDL_BUF_COUNT        1
    #define LV_SDL_FULLSCREEN       0
    #define LV_SDL_MOUSEWHEEL_MODE  LV_SDL_MOUSEWHEEL_MODE_CROP
#endif

/* ════════════════════════════════════════════════════════════════════
 * MISC
 * ════════════════════════════════════════════════════════════════════ */

#define LV_USE_LOG          1
#define LV_LOG_LEVEL        LV_LOG_LEVEL_INFO
#define LV_LOG_PRINTF       1

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_REFR_DEBUG   0

#define LV_USE_FS_STDIO     0   /* не нужен */
#define LV_USE_FS_POSIX     0
#define LV_USE_FS_WIN32     0
#define LV_USE_FS_FATFS     0
#define LV_USE_FS_MEMFS     0

#define LV_USE_SYSMON       0
#define LV_USE_PROFILER     0

/* Animations. */
#define LV_USE_ANIMATION    1
#define LV_USE_ANIMTIMELINE 1

#endif /* LV_CONF_H */
