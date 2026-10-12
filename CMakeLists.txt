cmake_minimum_required(VERSION 3.16)
project(ShaftTorsionCalculator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# AUTO - использовать Qt, если найден; ON - требовать Qt; OFF - собрать только консольную часть.
set(WITH_QT "AUTO" CACHE STRING "Использовать Qt для рисования эпюр (AUTO/ON/OFF)")
set_property(CACHE WITH_QT PROPERTY STRINGS AUTO ON OFF)

if(MSVC)
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# --- Ядро расчета (без Qt) ---
add_library(torsion_core STATIC src/TorsionModel.cpp)
target_include_directories(torsion_core PUBLIC src)

# --- Поиск Qt (этап 2) ---
set(QT_FOUND FALSE)
if(NOT WITH_QT STREQUAL "OFF")
    if(WITH_QT STREQUAL "ON")
        find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
    else()
        find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Widgets)
    endif()
    if(QT_FOUND)
        find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
        set(QT_FOUND TRUE)
    endif()
endif()

set(APP_SOURCES
    src/main.cpp
    src/ConsoleInterface.cpp
)

if(QT_FOUND)
    message(STATUS "Qt${QT_VERSION_MAJOR} найден: эпюры рисуются через QPainter")
    list(APPEND APP_SOURCES src/PlotterQt.cpp src/DiagramPainter.cpp)
    add_executable(ShaftTorsionCalculator ${APP_SOURCES})
    target_link_libraries(ShaftTorsionCalculator PRIVATE torsion_core Qt${QT_VERSION_MAJOR}::Widgets)
else()
    message(STATUS "Qt не найден: собирается только консольная часть (этап 1)")
    list(APPEND APP_SOURCES src/PlotterStub.cpp)
    add_executable(ShaftTorsionCalculator ${APP_SOURCES})
    target_link_libraries(ShaftTorsionCalculator PRIVATE torsion_core)
endif()

# --- Тесты ---
enable_testing()
add_executable(test_solver tests/test_solver.cpp)
target_link_libraries(test_solver PRIVATE torsion_core)
add_test(NAME solver COMMAND test_solver)
