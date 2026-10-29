import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The hotkey and how the popup over the game looks.
FormCard.FormCardPage {
    id: overlayPage

    required property OverlayOptions options
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

    title: qsTr("Popup and Hotkeys")

    FormCard.FormHeader {
        title: qsTr("Hotkeys")
    }

    FormCard.FormCard {
        KeyField {
            objectName: "checkKey"
            label: qsTr("Check an item")
            description: qsTr("Press it over an item in the game. An F key works best, as letters reach the game too.")
            key: overlayPage.options.checkKey
            onChosen: key => overlayPage.refused = !overlayPage.options.setCheckKey(key)
        }

        FormCard.FormDelegateSeparator {}

        KeyField {
            objectName: "resultsKey"
            label: qsTr("Show the trade results")
            description: qsTr("Opens the listings of your last trade search, and closes them again.")
            key: overlayPage.options.resultsKey
            onChosen: key => overlayPage.refused = !overlayPage.options.setResultsKey(key)
        }

        FormCard.FormDelegateSeparator {
            visible: refusedText.visible
        }

        FormCard.FormTextDelegate {
            id: refusedText
            objectName: "refusedText"
            visible: overlayPage.refused
            text: qsTr("That key is already taken.")
        }

        FormCard.FormDelegateSeparator {
            visible: errorText.visible
        }

        FormCard.FormTextDelegate {
            id: errorText
            objectName: "hotkeysError"
            visible: text !== ""
            text: overlayPage.options.hotkeysError
            textItem.wrapMode: Text.Wrap
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "advancedCopySwitch"
            text: qsTr("Use advanced copy")
            description: qsTr("Copies with Ctrl+Alt+C, which tells prefixes from suffixes, for crafting.")
            checked: overlayPage.options.advancedCopy
            onToggled: overlayPage.options.advancedCopy = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "clipboardSwitch"
            text: qsTr("Also check gear you copy yourself")
            description: qsTr("When you press Ctrl+C over it in the game.")
            checked: overlayPage.options.watchClipboard
            onToggled: overlayPage.options.watchClipboard = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "pausedSwitch"
            text: qsTr("Pause")
            description: qsTr("The hotkeys and the clipboard are ignored until you turn this off.")
            checked: overlayPage.options.paused
            onToggled: overlayPage.options.paused = checked
        }
    }

    FormCard.FormHeader {
        title: qsTr("Popup")
    }

    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            objectName: "positionBox"
            text: qsTr("Where it shows")
            textRole: "text"
            valueRole: "value"
            model: overlayPage.positions
            currentIndex: overlayPage.positions.findIndex(position => position.value === overlayPage.options.position)
            onActivated: overlayPage.options.position = currentValue
        }

        FormCard.FormDelegateSeparator {}

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

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "compactSwitch"
            text: qsTr("Compact")
            description: qsTr("With what hardly matters behind More.")
            checked: overlayPage.options.compact
            onToggled: overlayPage.options.compact = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "colorblindSwitch"
            text: qsTr("Colorblind friendly colors")
            description: qsTr("Blue for better and orange for worse, in place of green and red.")
            checked: overlayPage.options.colorblind
            onToggled: overlayPage.options.colorblind = checked
        }

        FormCard.FormDelegateSeparator {}

        NumberField {
            objectName: "autoHide"
            label: qsTr("Seconds until it hides")
            description: qsTr("It stays while the mouse is over it.")
            minimum: 2
            maximum: 120
            value: overlayPage.options.autoHideSeconds
            onEdited: value => overlayPage.options.autoHideSeconds = value
        }
    }

    FormCard.FormHeader {
        title: qsTr("General")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            objectName: "startSwitch"
            text: qsTr("Start with the desktop")
            description: qsTr("Waits in the tray when you log in, without the window.")
            checked: overlayPage.options.startWithDesktop
            onToggled: overlayPage.options.startWithDesktop = checked
        }

        FormCard.FormDelegateSeparator {
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
