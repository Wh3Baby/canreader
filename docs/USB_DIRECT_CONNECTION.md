# Поддержка прямого USB подключения

Если адаптер USB\VID_20A2&PID_0001 не создает виртуальный COM порт, требуется работа напрямую через USB.

## Варианты решения:

1. **Установить драйвер виртуального COM порта** (если доступен от производителя)
2. **Использовать libusb для прямого USB подключения**

## Для реализации прямого USB подключения:

1. Установить libusb:
   - Windows: скачать с https://libusb.info/
   - Linux: `sudo apt-get install libusb-1.0-0-dev`

2. Добавить в CMakeLists.txt:
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
target_link_libraries(${PROJECT_NAME} ${LIBUSB_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUSB_INCLUDE_DIRS})
```

3. Создать класс для работы с USB напрямую (аналогично CANInterface, но через libusb)

## Текущий статус:

Сейчас код работает только через виртуальный COM порт (QSerialPort).
Если адаптер не создает COM порт, нужно добавить поддержку libusb.

