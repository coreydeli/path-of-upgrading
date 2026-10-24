cmake_minimum_required(VERSION 3.21)

project(path-of-upgrading
    VERSION 0.1.0
    DESCRIPTION "Path of Exile 2 item overlay that checks items against your build"
    HOMEPAGE_URL "https://github.com/coreydeli/path-of-upgrading"
    LANGUAGES CXX)

# Used as the desktop file name, the Wayland app_id and the icon name.
set(APP_ID "io.github.coreydeli.PathOfUpgrading")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wcast-align -Wformat=2 -Wundef -Wvla
        -Wnon-virtual-dtor -Woverloaded-virtual -Wsuggest-override
        -Wzero-as-null-pointer-constant
        -Werror=return-type
        # designated initializers leave out what they don't set on purpose
        -Wno-missing-field-initializers
    )
endif()

add_compile_definitions(
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_URL_CAST_FROM_STRING
    QT_USE_QSTRINGBUILDER
    QT_NO_FOREACH
    QT_NO_KEYWORDS
)

# Debian 13, the oldest of the distributions with Plasma 6 still supported,
# has Qt 6.8
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Widgets Qml Quick QuickControls2)
# The window is Kirigami, which is only loaded at run time, as is the key
# recorder from KDeclarative. Debian 13 has these.
find_package(KF6Kirigami 6.13 REQUIRED)
find_package(KF6KirigamiAddons 1.7 REQUIRED)
find_package(KF6Declarative 6.13 REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)
# all the QML modules in one folder, which is where qmllint looks for them
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

option(BUILD_TESTING "Build the unit tests" ON)

add_subdirectory(src)
add_subdirectory(data)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
