@echo off
REM Скрипт для копирования необходимых Qt DLL файлов
REM Использование: copy_dlls_windows.bat [путь_к_qt_bin] [целевая_папка]

setlocal

REM Параметры по умолчанию
if "%1"=="" (
    echo Использование: copy_dlls_windows.bat [путь_к_qt_bin] [целевая_папка]
    echo Пример: copy_dlls_windows.bat C:\Qt\6.5.0\msvc2019_64\bin build_win\Release
    echo.
    echo Или установите переменную Qt6_DIR и скрипт попытается найти путь автоматически
    exit /b 1
)

set QT_BIN_DIR=%1
set TARGET_DIR=%2

REM Если целевая папка не указана, используем build_win\Release
if "%TARGET_DIR%"=="" (
    set TARGET_DIR=build_win\Release
)

REM Проверка существования папки Qt
if not exist "%QT_BIN_DIR%" (
    echo ОШИБКА: Папка Qt не найдена: %QT_BIN_DIR%
    exit /b 1
)

REM Создание целевой папки
if not exist "%TARGET_DIR%" (
    mkdir "%TARGET_DIR%"
)

echo Копирование Qt DLL файлов...
echo Из: %QT_BIN_DIR%
echo В: %TARGET_DIR%
echo.

REM Копирование основных DLL
copy "%QT_BIN_DIR%\Qt6Core.dll" "%TARGET_DIR%\" >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo [OK] Qt6Core.dll) else (echo [FAIL] Qt6Core.dll)

copy "%QT_BIN_DIR%\Qt6Gui.dll" "%TARGET_DIR%\" >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo [OK] Qt6Gui.dll) else (echo [FAIL] Qt6Gui.dll)

copy "%QT_BIN_DIR%\Qt6Widgets.dll" "%TARGET_DIR%\" >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo [OK] Qt6Widgets.dll) else (echo [FAIL] Qt6Widgets.dll)

copy "%QT_BIN_DIR%\Qt6SerialPort.dll" "%TARGET_DIR%\" >nul 2>&1
if %ERRORLEVEL% EQU 0 (echo [OK] Qt6SerialPort.dll) else (echo [FAIL] Qt6SerialPort.dll)

REM Копирование плагинов платформы
set PLATFORMS_SRC=%QT_BIN_DIR%\..\plugins\platforms
set PLATFORMS_DST=%TARGET_DIR%\platforms

if exist "%PLATFORMS_SRC%" (
    if not exist "%PLATFORMS_DST%" mkdir "%PLATFORMS_DST%"
    xcopy "%PLATFORMS_SRC%\*" "%PLATFORMS_DST%\" /E /I /Y >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo [OK] Плагины платформы скопированы
    ) else (
        echo [WARN] Не удалось скопировать плагины платформы
    )
)

echo.
echo Готово! DLL файлы скопированы в %TARGET_DIR%
echo.

endlocal

