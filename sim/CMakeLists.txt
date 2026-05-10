# sim/CMakeLists.txt
#
# Нативный SDL2-симулятор UI для macOS/Linux. Позволяет запустить
# полноценный LVGL-интерфейс без ESP32 — отлаживается, профилируется,
# делается скриншоты экранов.
#
# Использует ту же копию LVGL что и ESP-IDF (managed_components/lvgl__lvgl),
# чтобы не было version drift между симулятором и таргетом.
#
# Сборка (macOS, требуется brew install sdl2 cmake):
#   cd sim && ./build.sh && ./build/ro_hmi_sim
#
# Или из корня проекта:
#   cmake -S sim -B sim/build && cmake --build sim/build
#   ./sim/build/ro_hmi_sim

cmake_minimum_required(VERSION 3.20)
project(ro_hmi_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Определяем LVGL_LIVE_PREVIEW — заставляет ui_main.c стать пустым и
# активирует stub'ы в lvgl_preview.c (mock plant_data, alarm_ring, mqtt).
add_definitions(-DLVGL_LIVE_PREVIEW)

# LVGL config: указываем simple include и кладём наш sim/lv_conf.h в
# include path. lv_conf_internal.h сам подхватит его.
add_definitions(-DLV_CONF_INCLUDE_SIMPLE)

# ─── SDL2 (macOS: brew install sdl2; Linux: apt install libsdl2-dev) ─
# Сначала пробуем pkg-config, fallback — sdl2-config (всегда есть с brew/apt).
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 REQUIRED sdl2)
else()
    message(STATUS "pkg-config not found — fallback to sdl2-config")
    find_program(SDL2_CONFIG sdl2-config)
    if(NOT SDL2_CONFIG)
        message(FATAL_ERROR
            "Neither pkg-config nor sdl2-config found.\n"
            "macOS:  brew install sdl2 pkg-config\n"
            "Linux:  apt install libsdl2-dev pkg-config")
    endif()
    execute_process(COMMAND ${SDL2_CONFIG} --cflags
        OUTPUT_VARIABLE SDL2_CFLAGS_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${SDL2_CONFIG} --libs
        OUTPUT_VARIABLE SDL2_LIBS_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    # Превратить строки в списки CMake (split по пробелам).
    separate_arguments(SDL2_CFLAGS_LIST UNIX_COMMAND "${SDL2_CFLAGS_RAW}")
    separate_arguments(SDL2_LIBS_LIST   UNIX_COMMAND "${SDL2_LIBS_RAW}")
    set(SDL2_LIBRARIES ${SDL2_LIBS_LIST})
    # Для include-paths проще извлечь -I флаги и применить как include_directories.
    foreach(flag ${SDL2_CFLAGS_LIST})
        if(flag MATCHES "^-I(.+)")
            list(APPEND SDL2_INCLUDE_DIRS ${CMAKE_MATCH_1})
        endif()
    endforeach()
endif()

# ─── LVGL — берём source из managed_components ────────────────────
set(PROJ_ROOT  ${CMAKE_CURRENT_LIST_DIR}/..)
set(LVGL_ROOT  ${PROJ_ROOT}/managed_components/lvgl__lvgl)

# Все .c файлы LVGL рекурсивно (включая SDL backend в src/drivers/sdl/).
file(GLOB_RECURSE LVGL_SOURCES
    ${LVGL_ROOT}/src/*.c
)

# ─── UI sources проекта ───────────────────────────────────────────
set(MAIN_DIR ${PROJ_ROOT}/main)

file(GLOB UI_CORE_SOURCES   ${MAIN_DIR}/ui/*.c)
file(GLOB UI_SCREEN_SOURCES ${MAIN_DIR}/ui/screens/*.c)
file(GLOB I18N_SOURCES      ${MAIN_DIR}/i18n/*.c)
file(GLOB FONT_SOURCES      ${MAIN_DIR}/fonts/*.c)

# Header dirs:
include_directories(
    ${CMAKE_CURRENT_LIST_DIR}            # sim/lv_conf.h
    ${CMAKE_CURRENT_LIST_DIR}/stubs      # sim/stubs/esp_log.h, etc.
    ${LVGL_ROOT}                         # lvgl/lvgl.h, lvgl/src/...
    ${LVGL_ROOT}/src                     # SDL drivers in src/drivers/sdl
    ${MAIN_DIR}
    ${MAIN_DIR}/ui
    ${MAIN_DIR}/ui/screens
    ${MAIN_DIR}/i18n
    ${MAIN_DIR}/fonts
    ${MAIN_DIR}/data              # plant_data.h header (типы)
    ${MAIN_DIR}/board             # board.h (с LVGL_LIVE_PREVIEW stubs)
    ${MAIN_DIR}/mqtt              # mqtt_app.h (стабится в lvgl_preview.c)
    ${MAIN_DIR}/net
)

# ─── Executable ───────────────────────────────────────────────────
add_executable(ro_hmi_sim
    sim_main.c
    ${LVGL_SOURCES}
    ${UI_CORE_SOURCES}
    ${UI_SCREEN_SOURCES}
    ${I18N_SOURCES}
    ${FONT_SOURCES}
)

target_include_directories(ro_hmi_sim PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(ro_hmi_sim PRIVATE ${SDL2_LIBRARIES})
target_compile_options(ro_hmi_sim PRIVATE
    ${SDL2_CFLAGS_OTHER}
    -Wall -Wextra
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-format
    -Wno-deprecated-declarations
)

# Полезный output для отладки — кладём бинарь и compile_commands в sim/build/
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "")
message(STATUS "  RO HMI Simulator")
message(STATUS "  ───────────────────────────────")
message(STATUS "  LVGL source : ${LVGL_ROOT}")
message(STATUS "  SDL2 cflags : ${SDL2_CFLAGS}")
message(STATUS "  SDL2 libs   : ${SDL2_LIBRARIES}")
message(STATUS "  Build       : ${CMAKE_BUILD_TYPE}")
message(STATUS "")
