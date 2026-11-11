find_package(Qt6 REQUIRED COMPONENTS Network Widgets)
find_package(KF6StatusNotifierItem 6.0 REQUIRED)
# for the keyring
find_package(Qt6Keychain REQUIRED)

add_library(pou_platform STATIC
    autostart.cpp autostart.h
    copykeys.cpp copykeys.h
    gameinput.cpp gameinput.h
    gamewatcher.cpp gamewatcher.h
    gamewindow.cpp gamewindow.h
    globalshortcuts.cpp globalshortcuts.h
    kde/kdetray.cpp kde/kdetray.h
    keychain/keychainsecretstore.cpp keychain/keychainsecretstore.h
    localinstance.cpp localinstance.h
    logging.cpp logging.h
    notifier.cpp notifier.h
    opener.cpp opener.h
    overlay.cpp overlay.h
    packageinstaller.cpp packageinstaller.h
    restart.cpp restart.h
    screenmap.cpp screenmap.h
    secretstore.cpp secretstore.h
    showwindow.cpp showwindow.h
    singleinstance.cpp singleinstance.h
    tray.cpp tray.h
    traynotifier.cpp traynotifier.h
    # which needs no Windows, so it's tested everywhere
    win/winkeys.cpp win/winkeys.h
)

target_include_directories(pou_platform PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pou_platform
    PUBLIC Qt6::Gui Qt6Keychain::Qt6Keychain
    PRIVATE Qt6::Network Qt6::Widgets KF6::StatusNotifierItem
)

if(WIN32)
    target_sources(pou_platform PRIVATE
        win/wingameinput.cpp win/wingameinput.h
        win/winglobalshortcuts.cpp win/winglobalshortcuts.h
        win/wingamewatcher.cpp win/wingamewatcher.h
        win/winkeysender.cpp win/winkeysender.h
        win/winwindows.cpp win/winwindows.h
    )
    target_compile_definitions(pou_platform PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(pou_platform PRIVATE user32)
else()
    # The game runs under Proton as an X11 window, on a Wayland desktop through
    # Xwayland, so seeing it and pressing keys in it goes through the X server.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb xcb-keysyms xcb-randr xcb-xfixes xcb-xtest)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    find_package(KF6DBusAddons 6.0 REQUIRED)
    find_package(KF6GlobalAccel 6.0 REQUIRED)
    find_package(KF6Notifications 6.0 REQUIRED)
    find_package(KF6WindowSystem 6.0 REQUIRED)
    find_package(LayerShellQt 6.0 REQUIRED)

    target_sources(pou_platform PRIVATE
        kde/kdeglobalshortcuts.cpp kde/kdeglobalshortcuts.h
        kde/kdenotifier.cpp kde/kdenotifier.h
        packagekitinstaller.cpp packagekitinstaller.h
        xcb/xconnection.cpp xcb/xconnection.h
        xcb/xgameinput.cpp xcb/xgameinput.h
        xcb/xgamewatcher.cpp xcb/xgamewatcher.h
        xcb/xkeysender.cpp xcb/xkeysender.h
        xcb/xmonitors.cpp xcb/xmonitors.h
        xcb/xselectionwatcher.cpp xcb/xselectionwatcher.h
        xcb/xwindows.cpp xcb/xwindows.h
    )
    target_link_libraries(pou_platform
        PUBLIC PkgConfig::XCB Qt6::DBus
        PRIVATE KF6::DBusAddons KF6::GlobalAccel KF6::Notifications KF6::WindowSystem
                LayerShellQt::Interface
    )
endif()
