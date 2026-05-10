/**
 * @file sim/sim_main.c
 * @brief Entry point натвиного SDL2-симулятора UI.
 *
 * Создаёт SDL-окно 1280×800, инициализирует LVGL с SDL backend.
 *
 * ВАЖНО про SDL events: LVGL SDL backend сам опрашивает SDL_PollEvent
 * внутри lv_timer_handler(). Если параллельно вызывать SDL_PollEvent
 * в нашем event loop — мы будем выкидывать события из очереди ДО
 * того как LVGL до них дойдёт, и input не будет работать (мышь
 * "не кликабельна").
 *
 * Решение: используем SDL_AddEventWatch — callback вызывается при
 * каждом event'е НЕ удаляя его из очереди (return 1). Так детектим
 * SDL_QUIT (закрытие окна) и SDLK_ESCAPE, не мешая LVGL.
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

#define WIN_W   1280
#define WIN_H   800

extern void lvgl_live_preview_init(void);

/* ─── quit detection через SDL_AddEventWatch ────────────────────────
 * Watch получает копию event'а ДО того как тот попадёт в очередь.
 * Возвращаем 1 → event остаётся в очереди, LVGL его обработает.
 * Возвращаем 0 → event удаляется (нам не нужно). */
static volatile bool s_quit = false;

static int sim_event_watch(void *userdata, SDL_Event *event)
{
    (void)userdata;
    if (event->type == SDL_QUIT) {
        s_quit = true;
    } else if (event->type == SDL_KEYDOWN &&
               event->key.keysym.sym == SDLK_ESCAPE) {
        s_quit = true;
    }
    return 1;   /* НЕ потреблять event — оставить для LVGL */
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    fprintf(stderr, "[sim] Initializing LVGL + SDL2 backend %d×%d\n", WIN_W, WIN_H);

    lv_init();

    lv_display_t *disp = lv_sdl_window_create(WIN_W, WIN_H);
    if (!disp) {
        fprintf(stderr, "[sim] FATAL: lv_sdl_window_create() failed\n");
        return 1;
    }
    lv_sdl_window_set_title(disp, "RO HMI — LVGL Preview");

    lv_sdl_mouse_create();
    lv_sdl_mousewheel_create();
    lv_sdl_keyboard_create();

    /* Подписаться на SDL events для quit-detection БЕЗ потребления
     * (иначе LVGL не увидит мышь/клавиатуру). */
    SDL_AddEventWatch(sim_event_watch, NULL);

    lvgl_live_preview_init();

    fprintf(stderr, "[sim] UI initialized. Press Esc or ⌘Q to quit.\n");

    /* Главный цикл — НЕ дёргаем SDL_PollEvent самостоятельно.
     * lv_timer_handler внутри сам опрашивает SDL события и доводит
     * их до mouse/keyboard indev'ов. */
    while (!s_quit) {
        uint32_t until_next = lv_timer_handler();
        if (until_next > 30) until_next = 30;
        SDL_Delay(until_next);
    }

    fprintf(stderr, "[sim] Shutting down.\n");
    SDL_DelEventWatch(sim_event_watch, NULL);
    lv_deinit();
    SDL_Quit();
    return 0;
}
