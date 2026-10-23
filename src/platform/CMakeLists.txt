# The game runs under Proton as an X11 window, on a Wayland desktop through
# Xwayland, so seeing it and pressing keys in it goes through the X server.
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb xcb-keysyms xcb-randr xcb-xfixes xcb-xtest)

find_package(Qt6 REQUIRED COMPONENTS DBus)
find_package(KF6GlobalAccel 6.0 REQUIRED)
find_package(LayerShellQt 6.0 REQUIRED)

add_library(pou_platform STATIC
    copykeys.cpp copykeys.h
    gameinput.cpp gameinput.h
    gamewindow.cpp gamewindow.h
    globalshortcuts.cpp globalshortcuts.h
    kde/kdeglobalshortcuts.cpp kde/kdeglobalshortcuts.h
    logging.cpp logging.h
    overlay.cpp overlay.h
    screenmap.cpp screenmap.h
    xcb/xconnection.cpp xcb/xconnection.h
    xcb/xgameinput.cpp xcb/xgameinput.h
    xcb/xkeysender.cpp xcb/xkeysender.h
    xcb/xmonitors.cpp xcb/xmonitors.h
    xcb/xselectionwatcher.cpp xcb/xselectionwatcher.h
    xcb/xwindows.cpp xcb/xwindows.h
)

target_include_directories(pou_platform PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pou_platform
    PUBLIC Qt6::Gui PkgConfig::XCB
    PRIVATE Qt6::DBus KF6::GlobalAccel LayerShellQt::Interface
)
