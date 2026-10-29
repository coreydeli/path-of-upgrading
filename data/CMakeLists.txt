install(FILES ${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES icons/${APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

# The desktop file and the metainfo are checked when their checkers are
# installed.
if(BUILD_TESTING)
    enable_testing()
    find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
    if(DESKTOP_FILE_VALIDATE)
        add_test(NAME desktopfile
            COMMAND ${DESKTOP_FILE_VALIDATE} ${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.desktop)
    endif()
    find_program(APPSTREAMCLI appstreamcli)
    if(APPSTREAMCLI)
        add_test(NAME metainfo
            COMMAND ${APPSTREAMCLI} validate --no-net ${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.metainfo.xml)
    endif()
endif()
