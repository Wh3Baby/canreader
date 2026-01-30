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
    src/diagnosticprotocol.cpp
    src/udsprotocol.cpp
    src/obd2protocol.cpp
)

set(HEADERS
    include/mainwindow.h
    include/caninterface.h
    include/diagnosticprotocol.h
    include/udsprotocol.h
    include/obd2protocol.h
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

