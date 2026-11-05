# The app itself: the windows in QML and what they show in C++
qt_add_library(pou_app STATIC
    activecharacter.cpp activecharacter.h
    app.cpp app.h
    appearance.cpp appearance.h
    buildimporter.cpp buildimporter.h
    characterlist.cpp characterlist.h
    checkeditems.cpp checkeditems.h
    commandline.cpp commandline.h
    desktop.cpp desktop.h
    fonts.cpp fonts.h
    gearlist.cpp gearlist.h
    gearscoring.cpp gearscoring.h
    hotkeys.cpp hotkeys.h
    ignoredstats.cpp ignoredstats.h
    itemchecker.cpp itemchecker.h
    itemhistory.cpp itemhistory.h
    itemjudge.cpp itemjudge.h
    itempopup.cpp itempopup.h
    itemreader.cpp itemreader.h
    itemreport.cpp itemreport.h
    listingjudge.cpp listingjudge.h
    overlayoptions.cpp overlayoptions.h
    poboptions.cpp poboptions.h
    popuphider.cpp popuphider.h
    recommendedupgrades.cpp recommendedupgrades.h
    resistances.cpp resistances.h
    resultspanel.cpp resultspanel.h
    settingsstore.cpp settingsstore.h
    statpicker.cpp statpicker.h
    statweights.cpp statweights.h
    tradefinder.cpp tradefinder.h
    tradeoptions.cpp tradeoptions.h
    tradereport.cpp tradereport.h
    traderesults.cpp traderesults.h
    tradesession.cpp tradesession.h
)

# a singleton, so the windows share its colors
set_source_files_properties(qml/Style.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(pou_app
    URI PathOfUpgrading
    QML_FILES
        qml/AppearancePage.qml
        qml/ChangesTable.qml
        qml/ColumnsPage.qml
        qml/CharactersPage.qml
        qml/CheckedItemsPage.qml
        qml/FontPickerDialog.qml
    qml/FormRow.qml
        qml/GearPage.qml
        qml/GearScorePage.qml
        qml/IgnoredStatsPage.qml
        qml/KeyField.qml
        qml/Main.qml
        qml/NumberBox.qml
        qml/NumberField.qml
        qml/Ornament.qml
        qml/OrnateFrame.qml
        qml/OverlayPage.qml
        qml/PageHeader.qml
        qml/PobPage.qml
        qml/PopupCard.qml
        qml/PopupWindow.qml
        qml/RecommendationCard.qml
        qml/RecommendationRow.qml
        qml/ResultRow.qml
        qml/ResultsCard.qml
        qml/ResultsWindow.qml
        qml/ScoreGauge.qml
        qml/StatPickerDialog.qml
        qml/StatWeightsPage.qml
        qml/SlotRow.qml
        qml/SectionHeader.qml
        qml/SidebarHeader.qml
        qml/SectionText.qml
        qml/Style.qml
        qml/SwitchRow.qml
        qml/ToneLabel.qml
        qml/TonePill.qml
        qml/TradePage.qml
        qml/WindowGlow.qml
)

# the heading font, which the program carries
qt_add_resources(pou_app fonts
    PREFIX /fonts
    BASE ${PROJECT_SOURCE_DIR}/data/fonts
    FILES ${PROJECT_SOURCE_DIR}/data/fonts/Cinzel.ttf
)

target_link_libraries(pou_app PUBLIC
    pou_core
    pou_platform
    pou_services
    Qt6::Qml
    Qt6::Quick
    KF6::ColorScheme
)
