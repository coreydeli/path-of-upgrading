# The app itself: the windows in QML and what they show in C++
qt_add_library(pou_app STATIC)

qt_add_qml_module(pou_app
    URI PathOfUpgrading
    QML_FILES
        qml/Main.qml
)

target_link_libraries(pou_app PUBLIC
    pou_core
    Qt6::Qml
    Qt6::Quick
)
