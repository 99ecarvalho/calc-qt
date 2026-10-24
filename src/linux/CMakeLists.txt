# Linux build of Windows Calculator.
# The calculation engine (CalcManager) is built from the upstream sources, unchanged.
# The UWP/WinUI front end is replaced by a Qt 6 Widgets front end in app/.

cmake_minimum_required(VERSION 3.21)
project(ms-calculator-linux VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(UPSTREAM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(CALCMANAGER_DIR ${UPSTREAM_SRC}/CalcManager)
set(RESOURCES_DIR ${UPSTREAM_SRC}/Calculator/Resources)

# ---------------------------------------------------------------------------
# CalcManager: the arbitrary precision engine (Ratpack + CEngine + manager).
# UnitConverter is left out: it depends on the Windows PPL task library and
# its data loaders live in the C# view models.
# ---------------------------------------------------------------------------
file(GLOB CALCMANAGER_SOURCES CONFIGURE_DEPENDS
    ${CALCMANAGER_DIR}/*.cpp
    ${CALCMANAGER_DIR}/CEngine/*.cpp
    ${CALCMANAGER_DIR}/Ratpack/*.cpp)
list(FILTER CALCMANAGER_SOURCES EXCLUDE REGEX "/(pch|UnitConverter)\\.cpp$")

add_library(CalcManager STATIC ${CALCMANAGER_SOURCES})
target_include_directories(CalcManager PUBLIC
    ${CALCMANAGER_DIR}
    ${CALCMANAGER_DIR}/Ratpack
    ${CMAKE_CURRENT_SOURCE_DIR}/compat)
target_compile_definitions(CalcManager PUBLIC UNICODE _UNICODE)
target_compile_options(CalcManager PUBLIC
    -include ${CMAKE_CURRENT_SOURCE_DIR}/compat/linux_compat.h)
# Upstream code is written for MSVC warning levels; keep our own build quiet.
target_compile_options(CalcManager PRIVATE -w)

# ---------------------------------------------------------------------------
# Qt front end
# ---------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(ms-calculator
    app/main.cpp
    app/EngineResources.cpp
    app/EngineResources.h
    app/CalcBridge.cpp
    app/CalcBridge.h
    app/FitLabel.cpp
    app/FitLabel.h
    app/MainWindow.cpp
    app/MainWindow.h)
target_link_libraries(ms-calculator PRIVATE CalcManager Qt6::Widgets)
target_compile_options(ms-calculator PRIVATE -Wall -Wextra -Wno-unknown-pragmas)
target_compile_definitions(ms-calculator PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

# Engine strings come straight from the upstream .resw files, every locale.
# BASE strips the directory, so they are served as :/engine/<locale>/CEngineStrings.resw.
file(GLOB ENGINE_STRING_FILES CONFIGURE_DEPENDS ${RESOURCES_DIR}/*/CEngineStrings.resw)
qt_add_resources(ms-calculator engine_strings
    PREFIX /engine
    BASE ${RESOURCES_DIR}
    FILES ${ENGINE_STRING_FILES})

qt_add_resources(ms-calculator app_icon
    PREFIX /icons
    BASE ${UPSTREAM_SRC}/Calculator/Assets
    FILES ${UPSTREAM_SRC}/Calculator/Assets/CalculatorAppList.targetsize-256_altform-unplated.png
          ${UPSTREAM_SRC}/Calculator/Assets/CalculatorIcons.ttf)

# ---------------------------------------------------------------------------
# Tests: drive the engine headless and check results.
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE CalcManager)
    add_test(NAME engine_tests COMMAND engine_tests)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS ms-calculator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/ms-calculator.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${UPSTREAM_SRC}/Calculator/Assets/CalculatorAppList.targetsize-256_altform-unplated.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    RENAME ms-calculator.png)
