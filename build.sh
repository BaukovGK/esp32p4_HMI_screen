#!/bin/bash
# Сборка HMI под Waveshare ESP32-P4-NANO.
#
# Скрипт автоматически переключает target на esp32p4 при первой сборке
# или после смены ветки. Решает проблему: ESP-IDF при наличии build/
# использует кеш target (часто esp32 по умолчанию), и `set(IDF_TARGET)`
# в CMakeLists.txt не помогает — нужно `idf.py set-target` или ручная
# очистка кеша.
#
# Использование:
#   ./build.sh             — собрать
#   ./build.sh flash       — собрать и залить
#   ./build.sh monitor     — собрать, залить, открыть монитор
#   ./build.sh clean       — полная очистка (build/, sdkconfig)

set -e
cd "$(dirname "$0")"

EXPECTED_TARGET="esp32p4"

# Полная очистка по запросу
if [ "$1" = "clean" ]; then
    echo "Полная очистка: build/, sdkconfig, sdkconfig.old"
    rm -rf build sdkconfig sdkconfig.old
    exit 0
fi

# Проверяем нужно ли переключать target
NEED_SET_TARGET=0

if [ ! -d build ] || [ ! -f sdkconfig ]; then
    NEED_SET_TARGET=1
elif ! grep -q "CONFIG_IDF_TARGET=\"$EXPECTED_TARGET\"" sdkconfig 2>/dev/null; then
    echo "Текущий sdkconfig НЕ для $EXPECTED_TARGET, переключаюсь..."
    NEED_SET_TARGET=1
fi

if [ "$NEED_SET_TARGET" = "1" ]; then
    echo "Очистка кеша + установка target=$EXPECTED_TARGET..."
    rm -rf build sdkconfig
    idf.py set-target "$EXPECTED_TARGET"
fi

# Сборка / flash / monitor — передаём всё что после имени скрипта
if [ -z "$1" ]; then
    idf.py build
else
    idf.py "$@"
fi
