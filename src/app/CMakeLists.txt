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
    equipmentlist.cpp equipmentlist.h
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
    onboarding.cpp onboarding.h
    ownstyle.cpp ownstyle.h
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
        qml/CharacterPanel.qml
        qml/CharactersPage.qml
        qml/CheckedItemsPage.qml
        qml/EquipmentBoard.qml
        qml/EquipmentTile.qml
        qml/FontPickerDialog.qml
    qml/FormRow.qml
        qml/GearPage.qml
        qml/GearScorePage.qml
        qml/IgnoredStatsPage.qml
        qml/ItemPanel.qml
        qml/KeyField.qml
        qml/LeagueCard.qml
        qml/Main.qml
        qml/NumberBox.qml
        qml/NumberField.qml
        qml/OverlayPage.qml
        qml/PageHeader.qml
        qml/PlacementsTable.qml
        qml/PobPage.qml
        qml/PopupCard.qml
        qml/PopupWindow.qml
        qml/RecommendationCard.qml
        qml/RecommendationRow.qml
        qml/ResultRow.qml
        qml/ResultsCard.qml
        qml/ResultsWindow.qml
        qml/Notice.qml
        qml/Plate.qml
        qml/RowRule.qml
        qml/ScoreGauge.qml
        qml/StatPickerDialog.qml
        qml/StatWeightsPage.qml
        qml/SlotNotes.qml
        qml/SlotPanel.qml
        qml/SlotRow.qml
        qml/SectionHeader.qml
        qml/SidebarHeader.qml
        qml/SectionText.qml
        qml/SetupCharacterStep.qml
        qml/SetupGuide.qml
        qml/SetupLeagueStep.qml
        qml/SetupTradeStep.qml
        qml/Style.qml
        qml/SwitchRow.qml
        qml/TableHeader.qml
        qml/ToneLabel.qml
        qml/TonePill.qml
        qml/TradePage.qml
        qml/TradeStatus.qml
)

# the fonts the program carries
set(POU_FONTS
    BigShouldersDisplay-Black.ttf
    BigShouldersDisplay-Bold.ttf
    BigShouldersDisplay-ExtraBold.ttf
    BigShouldersText-Bold.ttf
    BigShouldersText-ExtraBold.ttf
    BigShouldersText-Medium.ttf
    BigShouldersText-Regular.ttf
    BigShouldersText-SemiBold.ttf
    Spectral-Bold.ttf
    Spectral-Italic.ttf
    Spectral-Medium.ttf
    Spectral-Regular.ttf
    Spectral-SemiBold.ttf
)
list(TRANSFORM POU_FONTS PREPEND ${PROJECT_SOURCE_DIR}/data/fonts/ OUTPUT_VARIABLE POU_FONT_FILES)
qt_add_resources(pou_app fonts
    PREFIX /fonts
    BASE ${PROJECT_SOURCE_DIR}/data/fonts
    FILES ${POU_FONT_FILES}
)

target_link_libraries(pou_app PUBLIC
    pou_core
    pou_platform
    pou_services
    pou_style
    pou_styleplugin
    Qt6::Qml
    Qt6::Quick
    KF6::ColorScheme
)
