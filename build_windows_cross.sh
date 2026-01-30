#!/bin/bash
# Скрипт кросс-компиляции для Windows на Linux

echo "========================================"
echo "Кросс-компиляция CAN Reader для Windows"
echo "========================================"
echo ""

# Проверка наличия mingw-w64
if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "ОШИБКА: mingw-w64 не установлен!"
    echo ""
    echo "Установите mingw-w64:"
    echo "  sudo apt-get install -y mingw-w64 g++-mingw-w64-x86-64"
    echo ""
    echo "Или для 32-bit:"
    echo "  sudo apt-get install -y mingw-w64 g++-mingw-w64-i686"
    exit 1
fi

echo "[1/5] Проверка компилятора..."
x86_64-w64-mingw32-g++ --version | head -1
echo ""

# Проверка Qt6 для Windows
echo "[2/5] Проверка Qt6 для Windows..."
QT6_FOUND=0

# Проверка MXE (M cross environment)
if [ -d "$HOME/mxe/usr/x86_64-w64-mingw32.static/qt6" ]; then
    export PATH="$HOME/mxe/usr/bin:$PATH"
    export Qt6_DIR="$HOME/mxe/usr/x86_64-w64-mingw32.static/qt6/lib/cmake/Qt6"
    echo "✓ Найден Qt6 в MXE (static)"
    QT6_FOUND=1
elif [ -d "$HOME/mxe/usr/x86_64-w64-mingw32.shared/qt6" ]; then
    export PATH="$HOME/mxe/usr/bin:$PATH"
    export Qt6_DIR="$HOME/mxe/usr/x86_64-w64-mingw32.shared/qt6/lib/cmake/Qt6"
    echo "✓ Найден Qt6 в MXE (shared)"
    QT6_FOUND=1
elif [ -n "$Qt6_DIR" ] && [ -d "$Qt6_DIR" ]; then
    echo "✓ Найден Qt6_DIR: $Qt6_DIR"
    QT6_FOUND=1
elif [ -d "/opt/qt6-windows/lib/cmake/Qt6" ]; then
    export Qt6_DIR="/opt/qt6-windows/lib/cmake/Qt6"
    echo "✓ Найден Qt6 в /opt/qt6-windows"
    QT6_FOUND=1
elif [ -d "$HOME/qt6-windows/lib/cmake/Qt6" ]; then
    export Qt6_DIR="$HOME/qt6-windows/lib/cmake/Qt6"
    echo "✓ Найден Qt6 в $HOME/qt6-windows"
    QT6_FOUND=1
else
    echo "ПРЕДУПРЕЖДЕНИЕ: Qt6 для Windows не найден!"
    if [ -d "$HOME/mxe" ]; then
        echo "MXE найден, но Qt6 еще не собран. Проверьте: ./check_qt6_build.sh"
    fi
    echo ""
fi

# Создание директории сборки
echo "[3/5] Создание директории сборки..."
rm -rf build_win_cross
mkdir -p build_win_cross
cd build_win_cross
echo ""

# Конфигурация CMake
echo "[4/5] Конфигурация CMake для кросс-компиляции..."
CMAKE_ARGS=(
    -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw.cmake
    -DCMAKE_BUILD_TYPE=Release
)

if [ $QT6_FOUND -eq 1 ] && [ -n "$Qt6_DIR" ]; then
    CMAKE_ARGS+=(-DQt6_DIR="$Qt6_DIR")
fi

cmake .. "${CMAKE_ARGS[@]}"

if [ $? -ne 0 ]; then
    echo ""
    echo "ОШИБКА: Конфигурация CMake не удалась!"
    echo ""
    echo "Возможные причины:"
    echo "  1. Qt6 для Windows не установлен"
    echo "  2. Неправильный путь к Qt6"
    echo "  3. Отсутствуют необходимые библиотеки"
    echo ""
    echo "Установите Qt6 для Windows или используйте сборку на Windows машине."
    cd ..
    exit 1
fi
echo ""

# Сборка
echo "[5/5] Сборка проекта..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "Сборка завершена успешно!"
    echo "========================================"
    echo ""
    if [ -f "CANReader.exe" ]; then
        echo "Исполняемый файл: build_win_cross/CANReader.exe"
        ls -lh CANReader.exe
    else
        echo "Исполняемый файл не найден. Проверьте вывод выше."
    fi
    echo ""
    echo "ПРИМЕЧАНИЕ: Для работы .exe файла нужны Qt6 DLL."
    echo "Скопируйте их из установки Qt6 для Windows."
else
    echo ""
    echo "ОШИБКА: Сборка не удалась!"
    cd ..
    exit 1
fi

cd ..

