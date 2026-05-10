/**
 * @file sim/sim_main.c
 * @brief Entry point натвиного SDL2-симулятора UI.
 *
 * Создаёт SDL-окно 1280×800 (как реальный экран Waveshare ESP32-P4-NANO),
 * инициализирует LVGL с SDL backend, запускает event loop:
 *   - lv_timer_handler() — обработка анимаций / таймеров / refresh
 *   - SDL обрабатывает мышь/клавиатуру через lv_sdl_window
 *   - Esc или ⌘Q закрывают окно
 *
 * UI собирает lvgl_live_preview_init() из main/ui/lvgl_preview.c —
 * там же mock plant_data, alarm_ring, MQTT stubs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "SDL.h"
#include "lvgl.h"
#include "drivers/sdl/lv_sdl_window.h"
#include "drivers/sdl/lv_sdl_mouse.h"
#include "drivers/sdl/lv_sdl_keyboard.h"
#include "drivers/sdl/lv_sdl_mousewheel.h"

/* Размеры экрана — должны совпадать с UI_SCREEN_W/H в ui_tokens.h. */
#define WIN_W   1280
#define WIN_H   800

/* Декларация UI entry point из main/ui/lvgl_preview.c.
 * Не подключаем header — функция объявляется extern напрямую. */
extern void lvgl_live_preview_init(void);

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    fprintf(stderr, "[sim] Initializing LVGL + SDL2 backend %d×%d\n", WIN_W, WIN_H);

    /* 1. Инициализация LVGL. */
    lv_init();

    /* 2. SDL display. lv_sdl_window_create создаёт окно и возвращает
     *    lv_display_t — LVGL внутри сам качает SDL_Init для VIDEO. */
    lv_display_t *disp = lv_sdl_window_create(WIN_W, WIN_H);
    if (!disp) {
        fprintf(stderr, "[sim] FATAL: lv_sdl_window_create() failed\n");
        return 1;
    }
    lv_sdl_window_set_title(disp, "RO HMI — LVGL Preview");

    /* 3. Input devices: мышь + колёсико + клавиатура. */
    lv_sdl_mouse_create();
    lv_sdl_mousewheel_create();
    lv_sdl_keyboard_create();

    /* 4. Зовём UI bootstrap — lvgl_live_preview_init создаёт чёрный фон,
     *    statusbar/tabbar (после миграции — TODO в lvgl_preview.c),
     *    стартовый экран (мнемосхема) и refresh-таймер. */
    lvgl_live_preview_init();

    fprintf(stderr, "[sim] UI initialized. Press Esc or ⌘Q to quit.\n");

    /* 5. Event loop. lv_timer_handler возвращает время до следующего
     *    события — спим столько, чтобы не жечь CPU зря. */
    bool running = true;
    while (running) {
        /* SDL events — закрытие окна / клавиши, остальное обрабатывает
         * сам LVGL через lv_sdl_*. */
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
                break;
            }
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
                break;
            }
        }

        uint32_t until_next = lv_timer_handler();
        if (until_next > 30) until_next = 30;
        SDL_Delay(until_next);
    }

    fprintf(stderr, "[sim] Shutting down.\n");
    lv_deinit();
    SDL_Quit();
    return 0;
}
