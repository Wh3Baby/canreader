# Инструкция по сборке проекта

## Установка зависимостей

### Linux (Kali/Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-serialport-dev \
    libqt6serialport6
```

### Проверка установки Qt6

```bash
pkg-config --modversion Qt6Core
```

## Сборка проекта

```bash
mkdir -p build
cd build
cmake ..
make
```

Исполняемый файл будет находиться в директории `build/` с именем `CANReader`.

## Запуск

```bash
./build/CANReader
```

**Примечание:** Для доступа к последовательным портам на Linux может потребоваться добавить пользователя в группу `dialout`:

```bash
sudo usermod -a -G dialout $USER
# После этого перелогиньтесь
```

