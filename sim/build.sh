#!/usr/bin/env bash
# sim/build.sh — собрать и запустить нативный SDL2-симулятор.
#
# Зависимости (macOS):
#   brew install sdl2 cmake pkg-config
#
# Зависимости (Linux Debian/Ubuntu):
#   sudo apt install libsdl2-dev cmake pkg-config build-essential
#
# Использование:
#   ./build.sh           # сборка + запуск
#   ./build.sh build     # только сборка
#   ./build.sh clean     # очистка build-директории
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

CMD="${1:-run}"

case "$CMD" in
    clean)
        echo "[sim] Cleaning build directory..."
        rm -rf build
        ;;
    build)
        echo "[sim] Configuring CMake..."
        cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
        echo "[sim] Building..."
        cmake --build build -j
        echo "[sim] Done. Run: ./build/ro_hmi_sim"
        ;;
    run|*)
        echo "[sim] Configuring CMake..."
        cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
        echo "[sim] Building..."
        cmake --build build -j
        echo "[sim] Launching simulator..."
        exec ./build/ro_hmi_sim
        ;;
esac
