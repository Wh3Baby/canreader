@echo off
REM Скрипт сборки проекта для Windows
REM Требуется: Qt6, CMake, компилятор (MSVC или MinGW)

echo ========================================
echo Сборка CAN Reader для Windows
echo ========================================
echo.

REM Проверка наличия CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ОШИБКА: CMake не найден в PATH!
    echo Установите CMake и добавьте его в PATH
    pause
    exit /b 1
)

echo [1/4] Проверка CMake...
cmake --version
echo.

REM Проверка наличия Qt6
if "%Qt6_DIR%"=="" (
    echo ПРЕДУПРЕЖДЕНИЕ: Переменная Qt6_DIR не установлена
    echo Попробуйте установить Qt6_DIR, например:
    echo set Qt6_DIR=C:\Qt\6.x.x\msvc2019_64\lib\cmake\Qt6
    echo.
)

REM Создание директории сборки
echo [2/4] Создание директории сборки...
if exist build_win rmdir /s /q build_win
mkdir build_win
cd build_win
echo.

REM Конфигурация CMake
echo [3/4] Конфигурация CMake...
echo Используется генератор по умолчанию (Visual Studio или MinGW)
cmake .. -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ОШИБКА: Конфигурация CMake не удалась!
    echo.
    echo Возможные решения:
    echo 1. Установите Qt6 и установите переменную Qt6_DIR
    echo 2. Убедитесь, что компилятор установлен (Visual Studio или MinGW)
    echo 3. Попробуйте указать генератор явно:
    echo    cmake .. -G "Visual Studio 17 2022" -A x64
    echo    или
    echo    cmake .. -G "MinGW Makefiles"
    echo.
    cd ..
    pause
    exit /b 1
)
echo.

REM Сборка
echo [4/4] Сборка проекта...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ОШИБКА: Сборка не удалась!
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Сборка завершена успешно!
echo ========================================
echo.
echo Исполняемый файл находится в:
if exist Release\CANReader.exe (
    echo   build_win\Release\CANReader.exe
) else if exist CANReader.exe (
    echo   build_win\CANReader.exe
) else (
    echo   build_win\ (проверьте конфигурацию)
)
echo.
cd ..
pause

