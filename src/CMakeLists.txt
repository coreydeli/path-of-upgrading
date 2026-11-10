add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(platform)
add_subdirectory(style)
add_subdirectory(app)

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
    KF6::I18nQml
)

if(WIN32)
    # The icon for Explorer and the Start menu, and the one the windows and
    # the tray have. Without a console, as it's started from the Start menu.
    find_package(KF6IconThemes 6.0 REQUIRED)
    target_sources(path-of-upgrading PRIVATE ${PROJECT_SOURCE_DIR}/packaging/windows/path-of-upgrading.rc)
    qt_add_resources(path-of-upgrading icon
        PREFIX /
        BASE ${PROJECT_SOURCE_DIR}/data
        FILES ${PROJECT_SOURCE_DIR}/data/icons/${APP_ID}.svg
    )
    target_link_libraries(path-of-upgrading PRIVATE KF6::IconThemes)
    set_target_properties(path-of-upgrading PROPERTIES WIN32_EXECUTABLE ON)
endif()

# Qt and Kirigami are shared and load their QML plugins themselves, and ours
# is linked in above, so there's nothing for the import scan to link.
set_target_properties(path-of-upgrading PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN ON)

install(TARGETS path-of-upgrading RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
