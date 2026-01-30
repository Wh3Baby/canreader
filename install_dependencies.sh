#!/bin/bash
# Скрипт установки зависимостей для сборки CAN Reader

echo "Установка зависимостей для CAN Reader..."

sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-serialport-dev \
    libqt6serialport6

if [ $? -eq 0 ]; then
    echo "Зависимости успешно установлены!"
    echo "Теперь можно собрать проект:"
    echo "  mkdir -p build && cd build && cmake .. && make"
else
    echo "Ошибка при установке зависимостей!"
    exit 1
fi

