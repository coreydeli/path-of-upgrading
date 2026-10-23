add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(platform)

qt_add_executable(path-of-upgrading
    main.cpp
)

qt_add_qml_module(path-of-upgrading
    URI PathOfUpgrading
    QML_FILES
        qml/Main.qml
)

target_compile_definitions(path-of-upgrading PRIVATE
    APP_ID="${APP_ID}"
    APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(path-of-upgrading PRIVATE
    pou_core
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

install(TARGETS path-of-upgrading RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
