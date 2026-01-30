#!/bin/bash
# Единый скрипт сборки для Windows: установка зависимостей, сборка Qt6 и проекта

set -e

MXE_DIR="$HOME/mxe"
BUILD_DIR="build_win"

echo "========================================"
echo "Сборка CAN Reader для Windows"
echo "========================================"
echo ""

# Функция проверки и установки зависимостей
check_dependencies() {
    echo "[1/7] Проверка зависимостей..."
    
    local missing_deps=()
    
    # Проверка системных зависимостей
    for dep in autopoint bison gperf intltool lzip cmake; do
        if ! command -v $dep &> /dev/null; then
            missing_deps+=($dep)
        fi
    done
    
    # Проверка mingw-w64
    if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
        missing_deps+=(mingw-w64)
    fi
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        echo "Не найдены зависимости: ${missing_deps[*]}"
        echo ""
        echo "Установите их командой:"
        echo "  sudo apt-get install -y autopoint bison gperf intltool lzip cmake g++-mingw-w64-x86-64"
        echo ""
        read -p "Установить сейчас? (требуется sudo) [y/N]: " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            sudo apt-get update
            sudo apt-get install -y autopoint bison gperf intltool lzip cmake g++-mingw-w64-x86-64
        else
            echo "Установите зависимости вручную и запустите скрипт снова"
            exit 1
        fi
    else
        echo "✓ Все зависимости установлены"
    fi
    echo ""
}

# Функция установки MXE
setup_mxe() {
    echo "[2/7] Проверка MXE..."
    
    if [ ! -d "$MXE_DIR" ]; then
        echo "MXE не найден. Клонирование..."
        cd "$HOME"
        git clone https://github.com/mxe/mxe.git
        cd - > /dev/null
        echo "✓ MXE клонирован"
    else
        echo "✓ MXE найден"
    fi
    echo ""
}

# Функция сборки Qt6
build_qt6() {
    echo "[3/7] Проверка Qt6 для Windows..."
    
    local qt6_found=0
    local qt6_path=""
    
    # Проверка Qt6 в MXE
    if [ -d "$MXE_DIR/usr/x86_64-w64-mingw32.static/qt6" ]; then
        qt6_path="$MXE_DIR/usr/x86_64-w64-mingw32.static/qt6"
        qt6_found=1
    elif [ -d "$MXE_DIR/usr/x86_64-w64-mingw32.shared/qt6" ]; then
        qt6_path="$MXE_DIR/usr/x86_64-w64-mingw32.shared/qt6"
        qt6_found=1
    fi
    
    if [ $qt6_found -eq 1 ]; then
        export PATH="$MXE_DIR/usr/bin:$PATH"
        export Qt6_DIR="$qt6_path/lib/cmake/Qt6"
        echo "✓ Qt6 найден в MXE"
        echo ""
        return 0
    fi
    
    # Qt6 не найден, нужно собрать
    echo "Qt6 не найден. Начинаю сборку через MXE..."
    echo "Это займет 30-60+ минут..."
    echo ""
    
    cd "$MXE_DIR"
    
    # Создаем settings.mk для указания только Windows target
    # Это пропустит сборку cmake для хост-системы
    echo "MXE_TARGETS := x86_64-w64-mingw32.static" > settings.mk
    echo "✓ Настроен MXE для сборки только Windows target"
    
    # Удаляем частичную сборку для хост-системы если она есть
    rm -rf usr/x86_64-pc-linux-gnu log/cmake_x86_64-pc-linux-gnu log/ninja_x86_64-pc-linux-gnu 2>/dev/null
    
    # Создаем симлинки на системные инструменты для хост-системы
    mkdir -p usr/x86_64-pc-linux-gnu/bin
    mkdir -p usr/x86_64-pc-linux-gnu/installed
    
    # Симлинки на системные инструменты
    [ -f /usr/bin/cmake ] && ln -sf /usr/bin/cmake usr/x86_64-pc-linux-gnu/bin/cmake 2>/dev/null
    [ -f /usr/bin/ninja ] && ln -sf /usr/bin/ninja usr/x86_64-pc-linux-gnu/bin/ninja 2>/dev/null
    
    # Фейковые метки установки для инструментов хост-системы
    touch usr/x86_64-pc-linux-gnu/installed/cmake
    touch usr/x86_64-pc-linux-gnu/installed/ninja
    
    # Фейковые метки для всех Qt6 модулей для хост-системы
    # Создаем метки для всех найденных Qt6 пакетов в MXE
    if [ -d src ]; then
        find src -name "qt6*.mk" 2>/dev/null | xargs -I {} basename {} .mk | while read pkg; do
            touch usr/x86_64-pc-linux-gnu/installed/$pkg 2>/dev/null
        done
    fi
    # Также создаем для основных модулей на всякий случай
    for pkg in qt6 qt6-qtbase qt6-qtserialport qt6-qtshadertools qt6-qtscxml qt6-qttools \
               qt6-qtdeclarative qt6-qtwebsockets qt6-qtsvg qt6-qtimageformats qt6-qt5compat \
               qt6-qtcharts qt6-qthttpserver qt6-qtservice qt6-qttranslations qt6-qtwebview; do
        touch usr/x86_64-pc-linux-gnu/installed/$pkg 2>/dev/null
    done
    
    # Создаем фейковую структуру Qt6 для хост-системы
    mkdir -p usr/x86_64-pc-linux-gnu/qt6/lib/cmake/Qt6
    mkdir -p usr/x86_64-pc-linux-gnu/qt6/libexec
    mkdir -p usr/x86_64-pc-linux-gnu/qt6/lib
    
    # Создаем фейковые библиотеки для основных Qt6 модулей
    for lib in Qt6Core Qt6Gui Qt6Widgets Qt6SerialPort Qt6Svg Qt6Network Qt6Qml Qt6Quick; do
        touch usr/x86_64-pc-linux-gnu/qt6/lib/lib${lib}.a 2>/dev/null
    done
    cat > usr/x86_64-pc-linux-gnu/qt6/lib/cmake/Qt6/Qt6Config.cmake << 'QT6_EOF'
# Fake Qt6Config.cmake for host system
set(Qt6_FOUND TRUE)
set(Qt6_VERSION "6.10.2")
set(Qt6_VERSION_MAJOR 6)
set(Qt6_VERSION_MINOR 10)
set(Qt6_VERSION_PATCH 2)
# Disable version check to prevent errors
set(QT_NO_PACKAGE_VERSION_CHECK TRUE)
QT6_EOF
    
    # Создаем Qt6ConfigVersion.cmake для совместимости
    cat > usr/x86_64-pc-linux-gnu/qt6/lib/cmake/Qt6/Qt6ConfigVersion.cmake << 'QT6VER_EOF'
# Fake Qt6ConfigVersion.cmake
set(PACKAGE_VERSION "6.10.2")
set(PACKAGE_VERSION_MAJOR 6)
set(PACKAGE_VERSION_MINOR 10)
set(PACKAGE_VERSION_PATCH 2)
set(PACKAGE_VERSION_EXACT TRUE)
set(PACKAGE_VERSION_COMPATIBLE TRUE)
QT6VER_EOF
    
    # Создаем фейковый qt-cmake-private
    cat > usr/x86_64-pc-linux-gnu/qt6/libexec/qt-cmake-private << 'CMAKE_EOF'
#!/bin/bash
# Fake qt-cmake-private that uses system cmake
exec /usr/bin/cmake "$@"
CMAKE_EOF
    chmod +x usr/x86_64-pc-linux-gnu/qt6/libexec/qt-cmake-private
    
    # Очищаем все временные файлы сборки Qt6 для хост-системы
    rm -rf tmp-qt6-*x86_64-pc-linux-gnu* log/qt6-*x86_64-pc-linux-gnu* 2>/dev/null
    
    echo "Сборка Qt6 для Windows (статическая версия)..."
    echo "Используется системный cmake, сборка только для Windows target"
    echo ""
    
    # Явно указываем target при сборке
    make MXE_TARGETS=x86_64-w64-mingw32.static qt6 -j$(nproc)
    
    cd - > /dev/null
    
    # Проверка результата
    if [ -d "$MXE_DIR/usr/x86_64-w64-mingw32.static/qt6" ]; then
        export PATH="$MXE_DIR/usr/bin:$PATH"
        export Qt6_DIR="$MXE_DIR/usr/x86_64-w64-mingw32.static/qt6/lib/cmake/Qt6"
        echo "✓ Qt6 успешно собран"
    elif [ -d "$MXE_DIR/usr/x86_64-w64-mingw32.shared/qt6" ]; then
        export PATH="$MXE_DIR/usr/bin:$PATH"
        export Qt6_DIR="$MXE_DIR/usr/x86_64-w64-mingw32.shared/qt6/lib/cmake/Qt6"
        echo "✓ Qt6 успешно собран"
    else
        echo "ОШИБКА: Не удалось собрать Qt6"
        exit 1
    fi
    echo ""
}

# Функция проверки компилятора
check_compiler() {
    echo "[4/7] Проверка компилятора..."
    if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
        echo "ОШИБКА: mingw-w64 компилятор не найден!"
        exit 1
    fi
    x86_64-w64-mingw32-g++ --version | head -1
    echo ""
}

# Функция сборки проекта
build_project() {
    echo "[5/7] Создание директории сборки..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    echo ""
    
    echo "[6/7] Конфигурация CMake..."
    CMAKE_ARGS=(
        -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw.cmake
        -DCMAKE_BUILD_TYPE=Release
    )
    
    if [ -n "$Qt6_DIR" ]; then
        CMAKE_ARGS+=(-DQt6_DIR="$Qt6_DIR")
    fi
    
    cmake .. "${CMAKE_ARGS[@]}"
    
    if [ $? -ne 0 ]; then
        echo ""
        echo "ОШИБКА: Конфигурация CMake не удалась!"
        cd ..
        exit 1
    fi
    echo ""
    
    echo "[7/7] Сборка проекта..."
    make -j$(nproc)
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "========================================"
        echo "Сборка завершена успешно!"
        echo "========================================"
        echo ""
        if [ -f "CANReader.exe" ]; then
            echo "Исполняемый файл: $BUILD_DIR/CANReader.exe"
            ls -lh CANReader.exe
        else
            echo "Исполняемый файл не найден"
        fi
        echo ""
    else
        echo ""
        echo "ОШИБКА: Сборка не удалась!"
        cd ..
        exit 1
    fi
    
    cd ..
}

# Основной процесс
check_dependencies
setup_mxe
build_qt6
check_compiler
build_project

echo "Готово!"

