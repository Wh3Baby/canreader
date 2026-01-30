# PowerShell скрипт сборки проекта для Windows
# Требуется: Qt6, CMake, компилятор (MSVC или MinGW)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Сборка CAN Reader для Windows" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Проверка наличия CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ОШИБКА: CMake не найден в PATH!" -ForegroundColor Red
    Write-Host "Установите CMake и добавьте его в PATH" -ForegroundColor Red
    Read-Host "Нажмите Enter для выхода"
    exit 1
}

Write-Host "[1/4] Проверка CMake..." -ForegroundColor Yellow
cmake --version
Write-Host ""

# Проверка Qt6
if (-not $env:Qt6_DIR) {
    Write-Host "ПРЕДУПРЕЖДЕНИЕ: Переменная Qt6_DIR не установлена" -ForegroundColor Yellow
    Write-Host "Попробуйте установить Qt6_DIR, например:" -ForegroundColor Yellow
    Write-Host '  $env:Qt6_DIR = "C:\Qt\6.x.x\msvc2019_64\lib\cmake\Qt6"' -ForegroundColor Yellow
    Write-Host ""
}

# Создание директории сборки
Write-Host "[2/4] Создание директории сборки..." -ForegroundColor Yellow
if (Test-Path "build_win") {
    Remove-Item -Recurse -Force "build_win"
}
New-Item -ItemType Directory -Path "build_win" | Out-Null
Set-Location "build_win"
Write-Host ""

# Конфигурация CMake
Write-Host "[3/4] Конфигурация CMake..." -ForegroundColor Yellow
Write-Host "Используется генератор по умолчанию (Visual Studio или MinGW)" -ForegroundColor Gray

$cmakeArgs = @("..", "-DCMAKE_BUILD_TYPE=Release")

# Попытка определить генератор
if (Get-Command cl -ErrorAction SilentlyContinue) {
    Write-Host "Найден MSVC, используем Visual Studio генератор" -ForegroundColor Gray
    $cmakeArgs += @("-G", "Visual Studio 17 2022", "-A", "x64")
} elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
    Write-Host "Найден MinGW, используем MinGW Makefiles" -ForegroundColor Gray
    $cmakeArgs += @("-G", "MinGW Makefiles")
}

& cmake $cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ОШИБКА: Конфигурация CMake не удалась!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Возможные решения:" -ForegroundColor Yellow
    Write-Host "1. Установите Qt6 и установите переменную Qt6_DIR" -ForegroundColor Yellow
    Write-Host "2. Убедитесь, что компилятор установлен (Visual Studio или MinGW)" -ForegroundColor Yellow
    Write-Host "3. Попробуйте указать генератор явно:" -ForegroundColor Yellow
    Write-Host '   cmake .. -G "Visual Studio 17 2022" -A x64' -ForegroundColor Gray
    Write-Host '   или' -ForegroundColor Gray
    Write-Host '   cmake .. -G "MinGW Makefiles"' -ForegroundColor Gray
    Write-Host ""
    Set-Location ..
    Read-Host "Нажмите Enter для выхода"
    exit 1
}
Write-Host ""

# Сборка
Write-Host "[4/4] Сборка проекта..." -ForegroundColor Yellow
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ОШИБКА: Сборка не удалась!" -ForegroundColor Red
    Set-Location ..
    Read-Host "Нажмите Enter для выхода"
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Сборка завершена успешно!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# Поиск исполняемого файла
$exePath = $null
if (Test-Path "Release\CANReader.exe") {
    $exePath = "build_win\Release\CANReader.exe"
} elseif (Test-Path "CANReader.exe") {
    $exePath = "build_win\CANReader.exe"
}

if ($exePath) {
    Write-Host "Исполняемый файл находится в:" -ForegroundColor Green
    Write-Host "  $exePath" -ForegroundColor Cyan
} else {
    Write-Host "Исполняемый файл не найден. Проверьте конфигурацию сборки." -ForegroundColor Yellow
}

Write-Host ""
Set-Location ..
Read-Host "Нажмите Enter для выхода"

