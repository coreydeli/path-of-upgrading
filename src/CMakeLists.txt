add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(platform)
add_subdirectory(app)

qt_add_executable(path-of-upgrading
    main.cpp
)

target_compile_definitions(path-of-upgrading PRIVATE
    APP_ID="${APP_ID}"
    APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(path-of-upgrading PRIVATE
    pou_appplugin
    Qt6::Widgets
    Qt6::QuickControls2
)

install(TARGETS path-of-upgrading RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
