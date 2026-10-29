add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(platform)
add_subdirectory(app)

find_package(KF6DBusAddons 6.0 REQUIRED)

qt_add_executable(path-of-upgrading
    main.cpp
)

target_compile_definitions(path-of-upgrading PRIVATE
    APP_ID="${APP_ID}"
    APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(path-of-upgrading PRIVATE
    pou_app
    pou_appplugin
    Qt6::Widgets
    Qt6::QuickControls2
    KF6::DBusAddons
    KF6::I18nQml
)

# Qt and Kirigami are shared and load their QML plugins themselves, and ours
# is linked in above, so there's nothing for the import scan to link.
set_target_properties(path-of-upgrading PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN ON)

install(TARGETS path-of-upgrading RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
