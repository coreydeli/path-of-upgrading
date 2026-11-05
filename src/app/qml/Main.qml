// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The app's window, with a page for each part of it in the sidebar.
Kirigami.ApplicationWindow {
    id: root

    required property App app
    // Starts with the window hidden, as when it starts with the desktop
    property bool startHidden: false

    // an Item, as Qt 6.8 warns that it can't check a Kirigami.Page argument
    function show(page: Item): void {
        pageStack.clear();
        pageStack.push(page);
    }

    width: Kirigami.Units.gridUnit * 46
    height: Kirigami.Units.gridUnit * 34
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 20
    visible: !startHidden
    title: qsTr("Path of Upgrading")

    // the entry that starts it can be removed while it waits in the tray
    onActiveChanged: if (active)
        app.overlayOptions.lookAtStart()

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: false
        // a sidebar when there's room for one
        modal: !root.wideScreen
        handleVisible: modal

        header: SidebarHeader {
            app: root.app
        }

        actions: [
            Kirigami.Action {
                objectName: "charactersAction"
                text: qsTr("Characters")
                icon.name: "user-identity"
                checkable: true
                checked: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(charactersPage)
            },
            Kirigami.Action {
                id: gearAction
                objectName: "gearAction"
                text: qsTr("Your Gear")
                icon.name: "view-list-details"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(gearPage)
            },
            Kirigami.Action {
                objectName: "gearScoreAction"
                text: qsTr("Gear Score")
                icon.name: "rating"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(gearScorePage)
            },
            Kirigami.Action {
                objectName: "weightsAction"
                text: qsTr("Stat Weights")
                icon.name: "view-statistics"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(weightsPage)
            },
            Kirigami.Action {
                objectName: "ignoredAction"
                text: qsTr("Ignored Stats")
                icon.name: "view-hidden"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(ignoredPage)
            },
            Kirigami.Action {
                objectName: "checkedAction"
                text: qsTr("Item History")
                icon.name: "view-history"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(checkedPage)
            },
            Kirigami.Action {
                objectName: "pobAction"
                text: qsTr("Build Analysis")
                icon.name: "office-chart-line"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(pobPage)
            },
            Kirigami.Action {
                objectName: "tradeAction"
                text: qsTr("Trade")
                icon.name: "taxes-finances"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(tradePage)
            },
            Kirigami.Action {
                objectName: "overlayAction"
                text: qsTr("Settings")
                icon.name: "settings-configure"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(overlayPage)
            },
            Kirigami.Action {
                objectName: "appearanceAction"
                text: qsTr("Appearance")
                icon.name: "preferences-desktop-theme"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(appearancePage)
            }
        ]
    }

    pageStack.initialPage: charactersPage
    // each page has its own title
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    QQC2.ActionGroup {
        id: pages
    }

    // the popup and the results window are in this engine too
    Binding {
        target: Style
        property: "appColors"
        value: root.app.appearance.appColors
    }

    Binding {
        target: Style
        property: "plain"
        value: root.app.appearance.plain
    }

    Binding {
        target: Style
        property: "headingFamily"
        value: root.app.appearance.headingFamily
    }

    SetupGuide {
        objectName: "setupGuide"
        app: root.app
        windowShown: root.visible
    }

    // The pages wait here while they aren't shown, so they keep where you
    // were on them.
    Item {
        visible: false

        CharactersPage {
            id: charactersPage
            app: root.app
            onGearAsked: gearAction.trigger()
        }

        GearPage {
            id: gearPage
            app: root.app
        }

        GearScorePage {
            id: gearScorePage
            scoring: root.app.gearScoring
        }

        StatWeightsPage {
            id: weightsPage
            weights: root.app.statWeights
            picker: root.app.statPicker
        }

        IgnoredStatsPage {
            id: ignoredPage
            ignored: root.app.ignoredStats
            picker: root.app.statPicker
        }

        CheckedItemsPage {
            id: checkedPage
            checkedItems: root.app.checkedItems
            options: root.app.overlayOptions
        }

        PobPage {
            id: pobPage
            app: root.app
        }

        TradePage {
            id: tradePage
            options: root.app.tradeOptions
            session: root.app.tradeSession
        }

        OverlayPage {
            id: overlayPage
            options: root.app.overlayOptions
        }

        AppearancePage {
            id: appearancePage
            appearance: root.app.appearance
        }
    }
}
