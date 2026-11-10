# The app's own Qt Quick Controls style, the same on every desktop and system:
# the controls in QML, and the plugin that gives Kirigami their colors
find_package(Qt6 REQUIRED COMPONENTS QuickControls2)

qt_add_library(pou_style STATIC)

set_source_files_properties(Look.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(pou_style
    URI PouStyle
    VERSION 1.0
    IMPORTS QtQuick.Controls.Basic
    QML_FILES
        Look.qml
        controls/Button.qml
        controls/ButtonFace.qml
        controls/ButtonText.qml
        controls/CheckBox.qml
        controls/ComboBox.qml
        controls/Field.qml
        controls/ItemDelegate.qml
        controls/Label.qml
        controls/Mark.qml
        controls/RadioButton.qml
        controls/ScrollBar.qml
        controls/SpinBox.qml
        controls/Switch.qml
        controls/TextField.qml
        controls/ToolButton.qml
        controls/ToolTip.qml
)

target_link_libraries(pou_style PUBLIC Qt6::Quick Qt6::QuickControls2 KF6::ColorScheme)

# Kirigami asks for a plugin named like the style, in its own folder of the
# plugins. It's built next to the program so a build folder runs as it is.
add_library(pou_stylekirigami MODULE pouplatform.cpp)
target_link_libraries(pou_stylekirigami PRIVATE KF6::KirigamiPlatform KF6::ColorScheme KF6::ConfigCore Qt6::Gui)
set_target_properties(pou_stylekirigami PROPERTIES
    OUTPUT_NAME PouStyle
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/src/kf6/kirigami/platform)
if(WIN32)
    # beside the program, where Qt looks
    install(TARGETS pou_stylekirigami LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}/kf6/kirigami/platform
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/kf6/kirigami/platform)
else()
    # in the distribution's Qt plugin folder, where Kirigami looks
    install(TARGETS pou_stylekirigami LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kf6/kirigami/platform)
endif()
