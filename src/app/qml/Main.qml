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

    // The group in the sidebar that holds a page, if one does
    function groupOf(page: Item): Kirigami.Action {
        if (page === charactersPage || page === gearScorePage || page === weightsPage || page === ignoredPage)
            return gearGroupAction;
        if (page === overlayPage || page === appearancePage || page === helpPage || page === aboutPage)
            return settingsGroupAction;
        return null;
    }

    // The drawer's row for an action, looking down from an item. Kirigami has no
    // call that goes into a submenu, so we press the row as you would.
    function rowOf(action: Kirigami.Action, from: var): var {
        if (from.tAction === action && from.trigger)
            return from;
        for (const child of from.children) {
            const found = rowOf(action, child);
            if (found)
                return found;
        }
        return null;
    }

    // an Item, as Qt 6.8 warns that it can't check a Kirigami.Page argument
    function show(page: Item): void {
        pageStack.clear();
        pageStack.push(page);
        // The sidebar stays on a group while you move between its pages, and
        // goes to the group of any other page, or to the top. A drawer you
        // pull out lets go either way.
        const group = groupOf(page);
        if (drawer.currentSubMenu !== group) {
            drawer.resetMenu();
            if (group)
                rowOf(group, drawer.contentItem)?.trigger();
        } else if (drawer.modal) {
            drawer.drawerOpen = false;
        }
    }

    width: Kirigami.Units.gridUnit * 46
    height: Kirigami.Units.gridUnit * 34
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 20
    visible: !startHidden
    title: qsTr("Path of Upgrading")

    // Whether the drawer is a sidebar, which is when there's room for one. Qt 6.8
    // stacks the dimmer of a modal popup beside its item, and Kirigami 6.13 moves
    // the item of a sidebar elsewhere, so the drawer must be closed before it
    // turns modal.
    property bool sidebar: wideScreen

    function followWidth(): void {
        if (wideScreen) {
            sidebar = true;
            globalDrawer.drawerOpen = true;
        } else if (!globalDrawer.visible) {
            sidebar = false;
        } else if (sidebar) {
            globalDrawer.drawerOpen = false;
        }
    }

    onWideScreenChanged: followWidth()

    // the entry that starts it can be removed while it waits in the tray
    onActiveChanged: if (active)
        app.overlayOptions.lookAtStart()

    globalDrawer: Kirigami.GlobalDrawer {
        id: drawer
        objectName: "sidebar"
        isMenu: false
        modal: !root.sidebar
        handleVisible: modal
        // show() decides when it goes back to the top
        resetMenuOnTriggered: false

        // closed, if it was closing to turn modal
        onVisibleChanged: root.followWidth()

        header: SidebarHeader {
            app: root.app
        }

        actions: [
            Kirigami.Action {
                id: gearAction
                objectName: "gearAction"
                text: qsTr("Your Gear")
                icon.name: "view-list-details"
                checkable: true
                checked: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(gearPage)
            },
            // the pages about how gear is judged and who wears it, one level down
            Kirigami.Action {
                id: gearGroupAction
                objectName: "gearGroupAction"
                text: qsTr("Gear Settings")
                icon.name: "rating"
                // lit while one of its pages is shown
                checked: charactersAction.checked || gearScoreAction.checked || weightsAction.checked || ignoredAction.checked

                Kirigami.Action {
                    id: charactersAction
                    objectName: "charactersAction"
                    text: qsTr("Import Character")
                    icon.name: "document-import"
                    checkable: true
                    QQC2.ActionGroup.group: pages
                    onTriggered: root.show(charactersPage)
                }

                Kirigami.Action {
                    id: gearScoreAction
                    objectName: "gearScoreAction"
                    text: qsTr("Gear Score")
                    icon.name: "rating"
                    checkable: true
                    QQC2.ActionGroup.group: pages
                    onTriggered: root.show(gearScorePage)
                }

                Kirigami.Action {
                    id: weightsAction
                    objectName: "weightsAction"
                    text: qsTr("Stat Weights")
                    icon.name: "view-statistics"
                    checkable: true
                    QQC2.ActionGroup.group: pages
                    onTriggered: root.show(weightsPage)
                }

                Kirigami.Action {
                    id: ignoredAction
                    objectName: "ignoredAction"
                    text: qsTr("Ignored Stats")
                    icon.name: "view-hidden"
                    checkable: true
                    QQC2.ActionGroup.group: pages
                    onTriggered: root.show(ignoredPage)
                }
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
                objectName: "tradeAction"
                text: qsTr("Trade")
                icon.name: "taxes-finances"
                visible: root.app.tradeOffered
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(tradePage)
            },
            Kirigami.Action {
                objectName: "pobAction"
                text: qsTr("Build Review")
                icon.name: "office-chart-line"
                checkable: true
                QQC2.ActionGroup.group: pages
                onTriggered: root.show(pobPage)
            },
            // popup and hotkeys, and how it looks, one level down
            Kirigami.Action {
                id: settingsGroupAction
                objectName: "settingsGroupAction"
                text: qsTr("Settings")
                icon.name: "settings-configure"
                // lit while one of its pages is shown
                checked: overlayAction.checked || appearanceAction.checked || helpAction.checked || aboutAction.checked

                Kirigami.Action {
                    id: overlayAction
                    objectName: "overlayAction"
                    text: qsTr("Settings")
                    icon.name: "settings-configure"
                    checkable: true
                    QQC2.ActionGroup.group: pages
                    onTriggered: root.show(overlayPage)
                }

                Kirigami.Action {
                    id: appearanceAction
                    objectName: "appearanceAction"
                    text: qsTr("Appearance")
                    icon.name: "preferences-desktop-theme"
                    checkable: true
                    QQC2.ActionGroup.group: pages
                    onTriggered: root.show(appearancePage)
                }

                Kirigami.Action {
                    id: helpAction
                    objectName: "helpAction"
                    text: qsTr("Help")
                    icon.name: "help-contents"
                    checkable: true
                    QQC2.ActionGroup.group: pages
                    onTriggered: root.show(helpPage)
                }

                Kirigami.Action {
                    id: aboutAction
                    objectName: "aboutAction"
                    text: qsTr("About")
                    icon.name: "help-about"
                    checkable: true
                    QQC2.ActionGroup.group: pages
                    onTriggered: root.show(aboutPage)
                }
            }
        ]
    }

    pageStack.initialPage: gearPage
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

    UpdateDialog {
        objectName: "updateDialog"
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
            tradeOffered: root.app.tradeOffered
            tradeOptions: root.app.tradeOptions
            updates: root.app.updates
        }

        AppearancePage {
            id: appearancePage
            appearance: root.app.appearance
        }

        HelpPage {
            id: helpPage
            help: root.app.help
        }

        AboutPage {
            id: aboutPage
            about: root.app.about
        }
    }
}
