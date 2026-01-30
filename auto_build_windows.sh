#!/bin/bash
# Автоматическая сборка проекта для Windows с отслеживанием прогресса

MXE_DIR="$HOME/mxe"
PROJECT_DIR="/home/wh3baby/Desktop/canreader"

echo "Автоматическая сборка CAN Reader для Windows"
echo "=============================================="
echo ""

# Функция проверки завершения сборки Qt6
check_qt6_built() {
    if [ -d "$MXE_DIR/usr/x86_64-w64-mingw32.static/qt6/lib/cmake/Qt6" ]; then
        return 0
    fi
    return 1
}

# Функция проверки сборки qt6-qtserialport
check_serialport_built() {
    if [ -f "$MXE_DIR/usr/x86_64-w64-mingw32.static/qt6/lib/cmake/Qt6SerialPort/Qt6SerialPortConfig.cmake" ]; then
        return 0
    fi
    return 1
}

# Шаг 1: Сборка Qt6 base
echo "[1/3] Проверка Qt6 base для Windows..."
if ! check_qt6_built; then
    echo "Сборка Qt6 base для Windows..."
    cd "$MXE_DIR"
    make MXE_TARGETS=x86_64-w64-mingw32.static qt6-qtbase -j$(nproc)
    if [ $? -ne 0 ]; then
        echo "ОШИБКА: Не удалось собрать Qt6 base"
        exit 1
    fi
    echo "✓ Qt6 base собран"
else
    echo "✓ Qt6 base уже собран"
fi
echo ""

# Шаг 2: Сборка qt6-qtserialport
echo "[2/3] Проверка Qt6SerialPort для Windows..."
if ! check_serialport_built; then
    echo "Сборка Qt6SerialPort для Windows..."
    cd "$MXE_DIR"
    make MXE_TARGETS=x86_64-w64-mingw32.static qt6-qtserialport -j$(nproc)
    if [ $? -ne 0 ]; then
        echo "ОШИБКА: Не удалось собрать Qt6SerialPort"
        exit 1
    fi
    echo "✓ Qt6SerialPort собран"
else
    echo "✓ Qt6SerialPort уже собран"
fi
echo ""

# Шаг 3: Сборка проекта
echo "[3/3] Сборка проекта CAN Reader..."
cd "$PROJECT_DIR"

export PATH="$MXE_DIR/usr/bin:$PATH"
export Qt6_DIR="$MXE_DIR/usr/x86_64-w64-mingw32.static/qt6/lib/cmake/Qt6"

mkdir -p build_win
cd build_win

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_DIR="$Qt6_DIR"

if [ $? -ne 0 ]; then
    echo "ОШИБКА: Конфигурация CMake не удалась"
    exit 1
fi

make -j$(nproc)

if [ $? -eq 0 ] && [ -f "CANReader.exe" ]; then
    echo ""
    echo "=============================================="
    echo "✓ Сборка завершена успешно!"
    echo "=============================================="
    echo ""
    echo "Исполняемый файл: build_win/CANReader.exe"
    ls -lh CANReader.exe
    echo ""
else
    echo "ОШИБКА: Сборка не удалась или файл не найден"
    exit 1
fi

