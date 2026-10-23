# The game runs under Proton as an X11 window, on a Wayland desktop through
# Xwayland, so seeing it and pressing keys in it goes through the X server.
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)

add_library(pou_platform STATIC
    gamewindow.cpp gamewindow.h
    xcb/xconnection.cpp xcb/xconnection.h
    xcb/xwindows.cpp xcb/xwindows.h
)

target_include_directories(pou_platform PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pou_platform PUBLIC Qt6::Core PkgConfig::XCB)
