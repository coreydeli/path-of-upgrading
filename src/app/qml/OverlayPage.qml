// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The hotkey and how the popup over the game looks.
ColumnsPage {
    id: overlayPage

    required property OverlayOptions options
    required property Updates updates
    // the results key is for trade searches
    property bool tradeOffered: true
    // the league, which has no page of its own without trade
    property TradeOptions tradeOptions
    // whether the last key you chose was refused
    property bool refused: false
    readonly property var positions: [
        {
            value: "cursor",
            text: qsTr("Next to the mouse")
        },
        {
            value: "left",
            text: qsTr("On the left of the screen")
        },
        {
            value: "right",
            text: qsTr("On the right of the screen")
        },
        {
            value: "top",
            text: qsTr("At the top of the screen")
        }
    ]

    title: qsTr("General")
    description: qsTr("The hotkeys, and how the popup over the game looks.")

    SectionHeader {
        title: qsTr("Hotkeys")
    }

    Sheet {
        KeyField {
            objectName: "checkKey"
            label: qsTr("Check an item")
            description: qsTr("Press it over an item in the game. An F key works best, as letters reach the game too.")
            key: overlayPage.options.checkKey
            onChosen: key => overlayPage.refused = !overlayPage.options.setCheckKey(key)
        }

        RowRule {
            visible: overlayPage.tradeOffered
        }

        KeyField {
            objectName: "resultsKey"
            visible: overlayPage.tradeOffered
            label: qsTr("Show the trade results")
            description: qsTr("Opens the listings of your last trade search, and closes them again.")
            key: overlayPage.options.resultsKey
            onChosen: key => overlayPage.refused = !overlayPage.options.setResultsKey(key)
        }

        RowRule {
            visible: gameOnlyText.visible
        }

        FormCard.FormTextDelegate {
            id: gameOnlyText
            objectName: "gameOnlyNote"
            visible: overlayPage.options.hotkeysFollowGame
            text: qsTr("These keys only work while Path of Exile 2 is running. The rest of the time other programs get them, so you can type them as usual.")
            textItem.wrapMode: Text.Wrap
        }

        RowRule {
            visible: refusedText.visible
        }

        FormCard.FormTextDelegate {
            id: refusedText
            objectName: "refusedText"
            visible: overlayPage.refused
            text: qsTr("That key is already taken.")
        }

        RowRule {
            visible: errorText.visible
        }

        FormCard.FormTextDelegate {
            id: errorText
            objectName: "hotkeysError"
            visible: text !== ""
            text: overlayPage.options.hotkeysError
            textItem.wrapMode: Text.Wrap
        }

        RowRule {}

        SwitchRow {
            objectName: "advancedCopySwitch"
            text: qsTr("Use advanced copy")
            description: qsTr("Copies with Ctrl+Alt+C, which tells prefixes from suffixes, for crafting.")
            checked: overlayPage.options.advancedCopy
            onToggled: overlayPage.options.advancedCopy = checked
        }

        RowRule {}

        SwitchRow {
            objectName: "clipboardSwitch"
            text: qsTr("Also check gear you copy yourself")
            description: qsTr("When you press Ctrl+C over it in the game.")
            checked: overlayPage.options.watchClipboard
            onToggled: overlayPage.options.watchClipboard = checked
        }

        RowRule {}

        SwitchRow {
            objectName: "pausedSwitch"
            text: qsTr("Pause")
            description: qsTr("The hotkeys and copied items are ignored until you turn this off.")
            checked: overlayPage.options.paused
            onToggled: overlayPage.options.paused = checked
        }
    }

    SectionHeader {
        title: qsTr("Popup")
    }

    Sheet {
        FormCard.FormComboBoxDelegate {
            objectName: "positionBox"
            text: qsTr("Where it shows")
            textRole: "text"
            valueRole: "value"
            model: overlayPage.positions
            currentIndex: overlayPage.positions.findIndex(position => position.value === overlayPage.options.position)
            onActivated: overlayPage.options.position = currentValue
        }

        RowRule {}

        FormCard.AbstractFormDelegate {
            background: null
            focusPolicy: Qt.NoFocus

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                QQC2.Label {
                    text: qsTr("Size")
                }

                QQC2.Slider {
                    objectName: "sizeSlider"
                    Layout.fillWidth: true
                    from: 0.5
                    to: 2
                    stepSize: 0.1
                    snapMode: QQC2.Slider.SnapAlways
                    value: overlayPage.options.scale
                    onMoved: overlayPage.options.scale = value
                    Accessible.name: qsTr("Size")
                }

                QQC2.Label {
                    objectName: "sizeText"
                    text: qsTr("%1%").arg(Math.round(overlayPage.options.scale * 100))
                    // so the slider doesn't move as the number grows
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 2.5
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        RowRule {}

        SwitchRow {
            objectName: "compactSwitch"
            text: qsTr("Compact")
            description: qsTr("Puts what hardly matters behind Show More.")
            checked: overlayPage.options.compact
            onToggled: overlayPage.options.compact = checked
        }

        RowRule {}

        SwitchRow {
            objectName: "colorblindSwitch"
            text: qsTr("Colorblind-friendly colors")
            description: qsTr("Blue for better and orange for worse, in place of green and red.")
            checked: overlayPage.options.colorblind
            onToggled: overlayPage.options.colorblind = checked
        }

        RowRule {}

        NumberField {
            objectName: "autoHide"
            label: qsTr("Popup Timeout")
            description: qsTr("How many seconds the popup stays up before it hides. "
                              + "It stays while the mouse is over it.")
            minimum: 2
            maximum: 120
            value: overlayPage.options.autoHideSeconds
            onEdited: value => overlayPage.options.autoHideSeconds = value
        }
    }

    SectionHeader {
        title: qsTr("League")
        visible: !overlayPage.tradeOffered
    }

    LeagueCard {
        visible: !overlayPage.tradeOffered
        options: overlayPage.tradeOptions
        tradeOffered: false
    }

    SectionHeader {
        title: qsTr("Updates")
    }

    Sheet {
        SwitchRow {
            objectName: "checkForUpdatesSwitch"
            text: qsTr("Check for updates when the app starts")
            description: qsTr("Asks GitHub once a day while it runs. Nothing is installed without you saying so.")
            checked: overlayPage.updates.checkOnLaunch
            onToggled: overlayPage.updates.checkOnLaunch = checked
        }

        RowRule {}

        SwitchRow {
            objectName: "includeBetasSwitch"
            text: qsTr("Include beta builds")
            description: qsTr("Betas come out often and may have bugs.")
            checked: overlayPage.updates.includeBetas
            onToggled: overlayPage.updates.includeBetas = checked
        }

        RowRule {}

        FormRow {
            text: qsTr("You have version %1").arg(overlayPage.updates.version)
            description: overlayPage.updates.status

            QQC2.Button {
                objectName: "checkNowButton"
                text: qsTr("Check Now")
                enabled: !overlayPage.updates.checking
                onClicked: overlayPage.updates.checkNow()
            }
        }
    }

    SectionHeader {
        title: qsTr("General")
    }

    Sheet {
        SwitchRow {
            objectName: "startSwitch"
            text: qsTr("Start with your desktop")
            description: qsTr("Waits in the tray when you log in, without the window.")
            checked: overlayPage.options.startWithDesktop
            onToggled: overlayPage.options.startWithDesktop = checked
        }

        RowRule {
            objectName: "startErrorSeparator"
            visible: startError.visible
        }

        FormCard.FormTextDelegate {
            id: startError
            objectName: "startError"
            visible: text !== ""
            text: overlayPage.options.startError
            textItem.wrapMode: Text.Wrap
        }
    }
}
