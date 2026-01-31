cmake_minimum_required(VERSION 3.16)
project(CANReader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets SerialPort)

# libusb
# Сначала проверяем переменные окружения (для CI/CD)
if(DEFINED ENV{LIBUSB_INCLUDE_DIR})
    set(LIBUSB_INCLUDE_DIR "$ENV{LIBUSB_INCLUDE_DIR}")
endif()
if(DEFINED ENV{LIBUSB_LIBRARY})
    set(LIBUSB_LIBRARY "$ENV{LIBUSB_LIBRARY}")
endif()

# Если не заданы через переменные окружения, ищем в стандартных местах
if(NOT LIBUSB_INCLUDE_DIR)
    find_path(LIBUSB_INCLUDE_DIR libusb-1.0/libusb.h
        PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
        "$ENV{TEMP}/libusb"
        "C:/libusb/include"
        "C:/Program Files/libusb/include"
        "$ENV{ProgramFiles}/libusb/include"
    )
endif()

if(NOT LIBUSB_LIBRARY)
    find_library(LIBUSB_LIBRARY
        NAMES usb-1.0 libusb-1.0
        PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        "$ENV{TEMP}/libusb"
        "C:/libusb/lib"
        "C:/Program Files/libusb/lib"
        "$ENV{ProgramFiles}/libusb/lib"
    )
endif()

if(NOT LIBUSB_INCLUDE_DIR OR NOT LIBUSB_LIBRARY)
    message(FATAL_ERROR "libusb не найден! USB подключение обязательно. Установите libusb или укажите пути через переменные окружения LIBUSB_INCLUDE_DIR и LIBUSB_LIBRARY")
else()
    set(USE_LIBUSB 1)
    message(STATUS "libusb найден:")
    message(STATUS "  Include: ${LIBUSB_INCLUDE_DIR}")
    message(STATUS "  Library: ${LIBUSB_LIBRARY}")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Исходные файлы
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/caninterface.cpp
    src/diagnosticprotocol.cpp
    src/udsprotocol.cpp
    src/obd2protocol.cpp
)

list(APPEND SOURCES src/usbdevice.cpp)

set(HEADERS
    include/mainwindow.h
    include/caninterface.h
    include/diagnosticprotocol.h
    include/udsprotocol.h
    include/obd2protocol.h
)

list(APPEND HEADERS include/usbdevice.h)

# Включаемые директории
include_directories(include)

# Создание исполняемого файла
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Подключение Qt библиотек
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::SerialPort
)

# Подключение libusb (обязательно)
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUSB_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${LIBUSB_LIBRARY})

# Для Windows
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    # Автоматическое копирование Qt DLL в папку сборки (опционально)
    # Раскомментируйте, если нужно автоматическое копирование DLL
    # add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #         $<TARGET_FILE:Qt6::Core>
    #         $<TARGET_FILE:Qt6::Gui>
    #         $<TARGET_FILE:Qt6::Widgets>
    #         $<TARGET_FILE:Qt6::SerialPort>
    #         $<TARGET_FILE_DIR:${PROJECT_NAME}>
    # )
endif()

