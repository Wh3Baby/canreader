# Кросс-компиляция для Windows на Linux

## Установка зависимостей

### 1. Установите MinGW-w64:

```bash
sudo apt-get update
sudo apt-get install -y \
    mingw-w64 \
    g++-mingw-w64-x86-64 \
    binutils-mingw-w64-x86-64
```

### 2. Установите Qt6 для Windows

**Проблема:** Для кросс-компиляции Qt6 приложения нужен Qt6 для Windows, который обычно устанавливается на Windows.

**Варианты решения:**

#### Вариант A: Скачать Qt6 для Windows и распаковать

1. Скачайте Qt6 для Windows с официального сайта:
   https://www.qt.io/download

2. Распакуйте в `/opt/qt6-windows` или другую директорию

3. Установите переменную окружения:
   ```bash
   export Qt6_DIR=/opt/qt6-windows/6.x.x/mingw_64/lib/cmake/Qt6
   ```

#### Вариант B: Использовать MXE (M cross environment)

MXE предоставляет готовую среду для кросс-компиляции с Qt:

```bash
git clone https://github.com/mxe/mxe.git
cd mxe
make qt6
```

Затем используйте:
```bash
export PATH=$(pwd)/usr/bin:$PATH
export Qt6_DIR=$(pwd)/usr/x86_64-w64-mingw32.static/qt6/lib/cmake/Qt6
```

#### Вариант C: Использовать готовый Qt6 из Windows установки

Если у вас есть доступ к Windows машине с установленным Qt6:
1. Скопируйте папку Qt6 (например, `C:\Qt\6.5.0\mingw_64`) на Linux
2. Установите `Qt6_DIR` на путь к этой папке

## Сборка

После установки всех зависимостей:

```bash
./build_windows_cross.sh
```

Или вручную:

```bash
mkdir -p build_win_cross
cd build_win_cross
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_DIR=/path/to/qt6-windows/lib/cmake/Qt6
make -j$(nproc)
```

## Альтернативный подход: Статическая линковка

Если у вас есть Qt6 со статическими библиотеками, можно собрать полностью статический .exe:

```bash
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_DIR=/path/to/qt6-static/lib/cmake/Qt6 \
    -DCMAKE_EXE_LINKER_FLAGS="-static"
```

## Проверка результата

После сборки проверьте файл:

```bash
file build_win_cross/CANReader.exe
```

Должно показать: `PE32+ executable (console) x86-64, for MS Windows`

## Примечания

- Кросс-компиляция Qt6 приложений сложна из-за необходимости Qt6 для Windows
- Рекомендуется собирать на Windows машине, если это возможно
- Альтернатива: использовать виртуальную машину Windows или WINE для сборки

