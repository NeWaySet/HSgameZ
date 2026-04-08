cmake_minimum_required(VERSION 3.16)

project(QtBattlegrounds VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Bluetooth)

add_executable(QtBattlegrounds
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/networkmanager.h
    src/networkmanager.cpp
    src/autobattler.h
    src/autobattler.cpp
)

target_link_libraries(QtBattlegrounds PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

if(TARGET Qt${QT_VERSION_MAJOR}::Bluetooth)
    target_link_libraries(QtBattlegrounds PRIVATE Qt${QT_VERSION_MAJOR}::Bluetooth)
    target_compile_definitions(QtBattlegrounds PRIVATE HAS_QT_BLUETOOTH=1)
endif()

target_compile_options(QtBattlegrounds PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QtBattlegrounds)
endif()
