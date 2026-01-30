# Сборка проекта для Windows

## Требования

### Обязательные компоненты:

1. **CMake 3.16 или выше**
   - Скачать: https://cmake.org/download/
   - Добавить в PATH при установке

2. **Qt 6.x для Windows**
   - Скачать: https://www.qt.io/download
   - Выберите версию с компилятором (MSVC или MinGW)
   - Рекомендуется: Qt 6.5.0 или выше

3. **Компилятор C++** (один из вариантов):

   **Вариант A: Visual Studio 2019/2022**
   - Visual Studio Community (бесплатно)
   - Скачать: https://visualstudio.microsoft.com/
   - При установке выберите "Разработка классических приложений на C++"

   **Вариант B: MinGW-w64**
   - Входит в состав Qt Installer (если выбран MinGW)
   - Или установить отдельно: https://www.mingw-w64.org/

## Установка Qt6

1. Запустите Qt Online Installer
2. Выберите версию Qt 6.x (например, 6.5.0)
3. Выберите компилятор:
   - **MSVC 2019 64-bit** (для Visual Studio)
   - **MinGW 11.2.0 64-bit** (для MinGW)
4. Установите компоненты:
   - Qt 6.x.x
   - Qt Creator (опционально)
   - Qt Serial Port

## Настройка переменных окружения

### Для Visual Studio (MSVC):

```cmd
set Qt6_DIR=C:\Qt\6.5.0\msvc2019_64\lib\cmake\Qt6
set PATH=%PATH%;C:\Qt\6.5.0\msvc2019_64\bin
```

### Для MinGW:

```cmd
set Qt6_DIR=C:\Qt\6.5.0\mingw_64\lib\cmake\Qt6
set PATH=%PATH%;C:\Qt\6.5.0\mingw_64\bin
```

**Примечание:** Замените `6.5.0` на вашу версию Qt и путь на актуальный.

### Постоянная установка переменных (опционально):

1. Откройте "Система" → "Дополнительные параметры системы"
2. Нажмите "Переменные среды"
3. Добавьте переменную `Qt6_DIR` со значением пути к Qt6

## Сборка проекта

### Способ 1: Использование скрипта (рекомендуется)

**Batch скрипт:**
```cmd
build_windows.bat
```

**PowerShell скрипт:**
```powershell
.\build_windows.ps1
```

### Способ 2: Ручная сборка

#### Для Visual Studio:

```cmd
mkdir build_win
cd build_win
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

#### Для MinGW:

```cmd
mkdir build_win
cd build_win
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Расположение исполняемого файла

После успешной сборки исполняемый файл будет находиться:

- **Visual Studio:** `build_win\Release\CANReader.exe`
- **MinGW:** `build_win\CANReader.exe`

## Запуск программы

1. Убедитесь, что Qt DLL файлы доступны:
   - Либо добавьте `C:\Qt\6.x.x\msvc2019_64\bin` (или `mingw_64\bin`) в PATH
   - Либо скопируйте необходимые DLL в папку с исполняемым файлом

2. Запустите `CANReader.exe`

## Необходимые DLL файлы

Программе требуются следующие DLL (находятся в `Qt\6.x.x\компилятор\bin`):

- `Qt6Core.dll`
- `Qt6Gui.dll`
- `Qt6Widgets.dll`
- `Qt6SerialPort.dll`
- И их зависимости

### Автоматическое копирование DLL (опционально)

Создайте скрипт `copy_dlls.bat`:

```batch
@echo off
set QT_DIR=C:\Qt\6.5.0\msvc2019_64\bin
set TARGET_DIR=build_win\Release

copy "%QT_DIR%\Qt6Core.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\Qt6Gui.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\Qt6Widgets.dll" "%TARGET_DIR%\"
copy "%QT_DIR%\Qt6SerialPort.dll" "%TARGET_DIR%\"

REM Копирование плагинов платформы
xcopy "%QT_DIR%\..\plugins\platforms" "%TARGET_DIR%\platforms\" /E /I /Y
```

## Решение проблем

### Ошибка: "Could not find a package configuration file provided by Qt6"

**Решение:**
1. Установите переменную `Qt6_DIR`
2. Убедитесь, что путь правильный
3. Проверьте, что Qt6 установлен с компонентом SerialPort

### Ошибка: "No CMAKE_CXX_COMPILER could be found"

**Решение:**
1. Установите Visual Studio или MinGW
2. Для Visual Studio запустите "Developer Command Prompt for VS"
3. Для MinGW добавьте его в PATH

### Ошибка при запуске: "The program can't start because Qt6Core.dll is missing"

**Решение:**
1. Добавьте папку `bin` Qt в PATH
2. Или скопируйте необходимые DLL в папку с программой

## Создание установочного пакета (опционально)

Для создания установочного пакета можно использовать:

- **Qt Installer Framework** (входит в состав Qt)
- **NSIS** (Nullsoft Scriptable Install System)
- **Inno Setup**

## Дополнительная информация

- Документация Qt: https://doc.qt.io/
- CMake документация: https://cmake.org/documentation/
- Visual Studio: https://visualstudio.microsoft.com/

