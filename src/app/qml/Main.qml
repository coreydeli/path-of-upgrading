pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The app's window, with a page for each part of it in the sidebar.
Kirigami.ApplicationWindow {
    id: root

    required property App app

    function show(page: Kirigami.Page) {
        pageStack.clear();
        pageStack.push(page);
    }

    width: Kirigami.Units.gridUnit * 46
    height: Kirigami.Units.gridUnit * 34
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 20
    visible: true
    title: qsTr("Path of Upgrading")

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: false
        // a sidebar when there's room for one
        modal: !root.wideScreen
        handleVisible: modal

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
                objectName: "pobAction"
                text: qsTr("Path of Building")
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
                text: qsTr("Popup and Hotkeys")
                icon.name: "preferences-desktop-keyboard-shortcut"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(overlayPage)
            }
        ]
    }

    pageStack.initialPage: charactersPage

    QQC2.ActionGroup {
        id: pages
    }

    // The pages wait here while they aren't shown, so they keep where you
    // were on them.
    Item {
        visible: false

        CharactersPage {
            id: charactersPage
            app: root.app
        }

        GearPage {
            id: gearPage
            app: root.app
        }

        GearScorePage {
            id: gearScorePage
            scoring: root.app.gearScoring
        }

        PobPage {
            id: pobPage
            app: root.app
        }

        TradePage {
            id: tradePage
            options: root.app.tradeOptions
        }

        OverlayPage {
            id: overlayPage
            options: root.app.overlayOptions
        }
    }
}
