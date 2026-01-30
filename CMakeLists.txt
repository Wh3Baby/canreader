cmake_minimum_required(VERSION 3.16)
project(CANReader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets SerialPort)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Исходные файлы
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/caninterface.cpp
)

set(HEADERS
    include/mainwindow.h
    include/caninterface.h
)

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

# Для Windows
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

