import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Which Path of Building the app runs and what matters when it judges items.
FormCard.FormCardPage {
    id: pobPage

    required property App app
    readonly property PobOptions options: app.pobOptions

    title: qsTr("Path of Building")

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormSwitchDelegate {
            objectName: "enabledSwitch"
            text: qsTr("Judge items with Path of Building")
            description: qsTr("By its own numbers for your build, once you've imported one.")
            checked: pobPage.options.enabled
            onToggled: pobPage.options.enabled = checked
        }
    }

    FormCard.FormHeader {
        title: qsTr("Where It Is")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            objectName: "foundText"
            text: pobPage.options.found !== "" ? pobPage.options.found : qsTr("Path of Building wasn't found.")
            description: pobPage.options.problem
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextFieldDelegate {
            id: folderField
            objectName: "folderField"
            label: qsTr("Folder")
            placeholderText: qsTr("The Flatpak, or the usual places")
            text: pobPage.options.location
            onEditingFinished: pobPage.options.location = text
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: qsTr("Choose a Folder")
            description: qsTr("For a Path of Building that isn't the Flatpak.")
            icon.name: "document-open-folder"
            onClicked: folderDialog.open()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "lookAgainButton"
            text: qsTr("Look Again")
            description: qsTr("Once you've installed it, or opened it for Path of Exile 2 the first time.")
            icon.name: "view-refresh"
            onClicked: pobPage.app.lookForPob()
        }
    }

    FormCard.FormHeader {
        title: qsTr("What Matters Most")
    }

    // These only weigh Path of Building's numbers, so they wait for it to be on.
    FormCard.FormCard {
        enabled: pobPage.options.enabled

        FormCard.AbstractFormDelegate {
            background: null
            focusPolicy: Qt.NoFocus

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    text: qsTr("Damage against survivability")
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    spacing: Kirigami.Units.largeSpacing

                    QQC2.Label {
                        text: qsTr("Survivability")
                    }

                    QQC2.Slider {
                        objectName: "damageSlider"
                        Layout.fillWidth: true
                        from: 0
                        to: 1
                        stepSize: 0.1
                        snapMode: QQC2.Slider.SnapAlways
                        value: pobPage.options.damageWeight
                        onMoved: pobPage.options.damageWeight = value
                        Accessible.name: qsTr("Damage against survivability")
                    }

                    QQC2.Label {
                        text: qsTr("Damage")
                    }
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "resistsSwitch"
            text: qsTr("Cap my resistances first")
            description: qsTr("While one is under the cap, items that fill it score much higher.")
            checked: pobPage.options.resistsFirst
            onToggled: pobPage.options.resistsFirst = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "chaosSwitch"
            text: qsTr("Count chaos resistance too")
            enabled: pobPage.options.resistsFirst
            checked: pobPage.options.chaos
            onToggled: pobPage.options.chaos = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "keepDamageSwitch"
            text: qsTr("Never lose damage")
            description: qsTr("An item that costs a good part of your damage is never an upgrade.")
            checked: pobPage.options.keepDamage
            onToggled: pobPage.options.keepDamage = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "spiritSwitch"
            text: qsTr("Never run short of Spirit")
            description: qsTr("For your minions and auras.")
            checked: pobPage.options.spirit
            onToggled: pobPage.options.spirit = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "attributesSwitch"
            text: qsTr("Keep attribute requirements met")
            description: qsTr("The Strength, Dexterity and Intelligence your gear and skills need.")
            checked: pobPage.options.attributes
            onToggled: pobPage.options.attributes = checked
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Path of Building's Folder")
        onAccepted: pobPage.options.setLocationUrl(selectedFolder)
    }
}
