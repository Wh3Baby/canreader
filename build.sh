#!/bin/bash
# Скрипт сборки проекта

echo "Сборка CAN Reader..."

# Проверка зависимостей
if ! pkg-config --exists Qt6Core; then
    echo "Ошибка: Qt6 не установлен!"
    echo "Запустите: ./install_dependencies.sh"
    exit 1
fi

# Создание директории сборки
mkdir -p build
cd build

# Конфигурация CMake
echo "Конфигурация CMake..."
cmake ..

if [ $? -ne 0 ]; then
    echo "Ошибка конфигурации CMake!"
    echo "Убедитесь, что установлены все зависимости: ./install_dependencies.sh"
    exit 1
fi

# Сборка
echo "Сборка проекта..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Сборка завершена успешно!"
    echo "Исполняемый файл: ./build/CANReader"
    echo "Запуск: ./build/CANReader"
else
    echo "Ошибка при сборке!"
    exit 1
fi
