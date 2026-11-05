cmake_minimum_required(VERSION 3.21)

project(path-of-upgrading
    VERSION 0.5.0
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
# The colors of the app are a KDE color scheme, which this applies
find_package(KF6ColorScheme 6.13 REQUIRED)
# Kirigami Addons translates its text with KI18n, which the window's engine
# is set up for
find_package(KF6I18n 6.13 REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)
# all the QML modules in one folder, which is where qmllint looks for them
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

option(BUILD_TESTING "Build the unit tests" ON)
# The release builds of a beta turn this on. Trade searches start on in a beta,
# and off in a release until they've been tested more, see isBetaBuild().
option(POU_BETA "Build a beta, which has trade searches on until you turn them off" OFF)

add_subdirectory(src)
add_subdirectory(data)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(NOT WIN32)
    include(packaging/debian.cmake)
endif()
