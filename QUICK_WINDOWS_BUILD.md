# Быстрая инструкция: Сборка .exe на Linux

## Проблема

Для кросс-компиляции Qt6 приложения на Linux нужны:
1. **MinGW-w64 компилятор** (можно установить)
2. **Qt6 для Windows** (основная проблема - обычно устанавливается только на Windows)

## Решение

### Вариант 1: Установить Qt6 для Windows вручную (рекомендуется)

1. **Установите MinGW-w64:**
   ```bash
   sudo apt-get update
   sudo apt-get install -y g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64
   ```

2. **Скачайте Qt6 для Windows:**
   - Перейдите на https://www.qt.io/download
   - Скачайте Qt 6.x для Windows (MinGW версию)
   - Распакуйте архив (например, в `/opt/qt6-windows`)

3. **Соберите проект:**
   ```bash
   export Qt6_DIR=/opt/qt6-windows/6.x.x/mingw_64/lib/cmake/Qt6
   ./build_windows_cross.sh
   ```

### Вариант 2: Использовать MXE (M cross environment)

MXE предоставляет готовую среду с Qt6:

```bash
# Установка MXE
git clone https://github.com/mxe/mxe.git
cd mxe

# Сборка Qt6 (занимает много времени)
make qt6

# Использование
export PATH=$(pwd)/usr/bin:$PATH
export Qt6_DIR=$(pwd)/usr/x86_64-w64-mingw32.static/qt6/lib/cmake/Qt6

# Сборка проекта
cd /path/to/canreader
./build_windows_cross.sh
```

### Вариант 3: Собрать на Windows машине (самый простой)

Если у вас есть доступ к Windows:
1. Склонируйте репозиторий на Windows
2. Установите Qt6 на Windows
3. Запустите `build_windows.bat`

### Вариант 4: Использовать Docker с Windows

Можно использовать Docker образ с Windows и Qt6 для сборки.

## Текущий статус

Для сборки на этой системе нужно:

1. ✅ Скрипты созданы (`build_windows_cross.sh`, `toolchain-mingw.cmake`)
2. ❌ MinGW-w64 компилятор не установлен (нужен sudo)
3. ❌ Qt6 для Windows не найден (нужно скачать)

## Команды для установки

```bash
# 1. Установите MinGW-w64
sudo apt-get install -y g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64

# 2. Скачайте Qt6 для Windows и распакуйте
# (вручную с https://www.qt.io/download)

# 3. Установите путь к Qt6
export Qt6_DIR=/path/to/qt6-windows/6.x.x/mingw_64/lib/cmake/Qt6

# 4. Соберите проект
./build_windows_cross.sh
```

## Альтернатива: Статическая сборка

Если у вас есть Qt6 со статическими библиотеками, можно собрать полностью статический .exe, который не требует DLL:

```bash
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_DIR=/path/to/qt6-static/lib/cmake/Qt6 \
    -DCMAKE_EXE_LINKER_FLAGS="-static"
```

## Примечание

Кросс-компиляция Qt6 приложений сложна из-за необходимости Qt6 для целевой платформы. 
**Рекомендуется собирать на Windows машине**, если это возможно.

