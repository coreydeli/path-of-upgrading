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

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(src)
