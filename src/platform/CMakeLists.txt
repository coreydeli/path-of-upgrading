# The game runs under Proton as an X11 window, on a Wayland desktop through
# Xwayland, so seeing it and pressing keys in it goes through the X server.
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb xcb-keysyms xcb-randr xcb-xfixes xcb-xtest)

find_package(Qt6 REQUIRED COMPONENTS DBus Widgets)
find_package(KF6GlobalAccel 6.0 REQUIRED)
find_package(KF6Notifications 6.0 REQUIRED)
find_package(KF6StatusNotifierItem 6.0 REQUIRED)
find_package(LayerShellQt 6.0 REQUIRED)
# for the keyring
find_package(Qt6Keychain REQUIRED)

add_library(pou_platform STATIC
    autostart.cpp autostart.h
    copykeys.cpp copykeys.h
    gameinput.cpp gameinput.h
    gamewindow.cpp gamewindow.h
    globalshortcuts.cpp globalshortcuts.h
    kde/kdeglobalshortcuts.cpp kde/kdeglobalshortcuts.h
    kde/kdenotifier.cpp kde/kdenotifier.h
    kde/kdetray.cpp kde/kdetray.h
    keychain/keychainsecretstore.cpp keychain/keychainsecretstore.h
    logging.cpp logging.h
    notifier.cpp notifier.h
    overlay.cpp overlay.h
    screenmap.cpp screenmap.h
    secretstore.cpp secretstore.h
    tray.cpp tray.h
    xcb/xconnection.cpp xcb/xconnection.h
    xcb/xgameinput.cpp xcb/xgameinput.h
    xcb/xkeysender.cpp xcb/xkeysender.h
    xcb/xmonitors.cpp xcb/xmonitors.h
    xcb/xselectionwatcher.cpp xcb/xselectionwatcher.h
    xcb/xwindows.cpp xcb/xwindows.h
)

target_include_directories(pou_platform PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pou_platform
    PUBLIC Qt6::Gui PkgConfig::XCB Qt6Keychain::Qt6Keychain
    PRIVATE Qt6::DBus Qt6::Widgets KF6::GlobalAccel KF6::Notifications KF6::StatusNotifierItem
           LayerShellQt::Interface
)
