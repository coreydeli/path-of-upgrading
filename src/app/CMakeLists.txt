# The app itself: the windows in QML and what they show in C++
qt_add_library(pou_app STATIC
    itemchecker.cpp itemchecker.h
    itemjudge.cpp itemjudge.h
    itempopup.cpp itempopup.h
    itemreader.cpp itemreader.h
    itemreport.cpp itemreport.h
    popuphider.cpp popuphider.h
)

qt_add_qml_module(pou_app
    URI PathOfUpgrading
    QML_FILES
        qml/ChangesTable.qml
        qml/Main.qml
        qml/PopupCard.qml
        qml/PopupWindow.qml
        qml/ToneLabel.qml
)

target_link_libraries(pou_app PUBLIC
    pou_core
    pou_platform
    pou_services
    Qt6::Qml
    Qt6::Quick
)
