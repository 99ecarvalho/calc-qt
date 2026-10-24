# Linux build of Windows Calculator.
# The calculation engine (CalcManager) is built from the upstream sources, unchanged.
# The UWP/WinUI front end is replaced by a Qt 6 Widgets front end in app/.

cmake_minimum_required(VERSION 3.21)
project(calc-qt VERSION 1.1.0 LANGUAGES CXX)

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
find_package(Python3 REQUIRED COMPONENTS Interpreter)
qt_standard_project_setup()

qt_add_executable(calc-qt
    app/main.cpp
    app/AppStrings.cpp
    app/AppStrings.h
    app/DateCalculator.cpp
    app/DateCalculator.h
    app/DatePage.cpp
    app/DatePage.h
    app/EngineResources.cpp
    app/EngineResources.h
    app/CalcBridge.cpp
    app/CalcBridge.h
    app/FitLabel.cpp
    app/FitLabel.h
    app/MainWindow.cpp
    app/MainWindow.h)
target_link_libraries(calc-qt PRIVATE CalcManager Qt6::Widgets)
target_compile_options(calc-qt PRIVATE -Wall -Wextra -Wno-unknown-pragmas)
target_compile_definitions(calc-qt PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

# Engine strings come straight from the upstream .resw files, every locale.
# BASE strips the directory, so they are served as :/engine/<locale>/CEngineStrings.resw.
file(GLOB ENGINE_STRING_FILES CONFIGURE_DEPENDS ${RESOURCES_DIR}/*/CEngineStrings.resw)
qt_add_resources(calc-qt engine_strings
    PREFIX /engine
    BASE ${RESOURCES_DIR}
    FILES ${ENGINE_STRING_FILES})

# UI strings (mode names, date calculation) from the upstream Resources.resw
# files, reduced at build time to the keys the app uses.
set(UI_STRINGS_JSON ${CMAKE_CURRENT_BINARY_DIR}/strings/ui_strings.json)
file(GLOB UI_RESW_FILES CONFIGURE_DEPENDS ${RESOURCES_DIR}/*/Resources.resw)
add_custom_command(
    OUTPUT ${UI_STRINGS_JSON}
    COMMAND Python3::Interpreter -I ${CMAKE_CURRENT_SOURCE_DIR}/tools/extract-strings.py ${RESOURCES_DIR} ${UI_STRINGS_JSON}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/extract-strings.py ${UI_RESW_FILES}
    COMMENT "Extracting UI strings from upstream Resources.resw"
    VERBATIM)
qt_add_resources(calc-qt ui_strings
    PREFIX /strings
    BASE ${CMAKE_CURRENT_BINARY_DIR}/strings
    FILES ${UI_STRINGS_JSON})

qt_add_resources(calc-qt ui_images
    PREFIX /icons
    BASE app/icons
    FILES app/icons/chevron-down.png app/icons/chevron-up.png)

qt_add_resources(calc-qt app_icon
    PREFIX /icons
    BASE packaging/icons
    FILES packaging/icons/calc-qt.png)
qt_add_resources(calc-qt icon_font
    PREFIX /icons
    BASE ${UPSTREAM_SRC}/Calculator/Assets
    FILES ${UPSTREAM_SRC}/Calculator/Assets/CalculatorIcons.ttf)

# ---------------------------------------------------------------------------
# Tests: drive the engine headless and check results.
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE CalcManager)
    add_test(NAME engine_tests COMMAND engine_tests)

    add_executable(date_tests tests/date_tests.cpp app/DateCalculator.cpp)
    target_include_directories(date_tests PRIVATE app)
    target_link_libraries(date_tests PRIVATE Qt6::Core)
    add_test(NAME date_tests COMMAND date_tests)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS calc-qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/calc-qt.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES packaging/calc-qt.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/icons/calc-qt.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
install(FILES packaging/icons/calc-qt.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
