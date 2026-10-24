# The app itself: the windows in QML and what they show in C++
qt_add_library(pou_app STATIC
    activecharacter.cpp activecharacter.h
    app.cpp app.h
    hotkeys.cpp hotkeys.h
    itemchecker.cpp itemchecker.h
    itemhistory.cpp itemhistory.h
    itemjudge.cpp itemjudge.h
    itempopup.cpp itempopup.h
    itemreader.cpp itemreader.h
    itemreport.cpp itemreport.h
    overlayoptions.cpp overlayoptions.h
    popuphider.cpp popuphider.h
    settingsstore.cpp settingsstore.h
    tradefinder.cpp tradefinder.h
    tradeoptions.cpp tradeoptions.h
    tradereport.cpp tradereport.h
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
