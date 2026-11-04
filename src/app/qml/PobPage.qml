// SPDX-License-Identifier: MIT
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

    property bool showPlaces: false

    title: qsTr("Path of Building")

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        SwitchRow {
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
            text: pobPage.options.headline
            description: pobPage.options.details.join(" ")
        }

        FormCard.FormDelegateSeparator {
            visible: installedAtText.visible
        }

        FormCard.FormTextDelegate {
            id: installedAtText
            objectName: "installedAtText"
            visible: pobPage.options.installedAt !== ""
            text: qsTr("Program files")
            description: pobPage.options.installedAt
        }

        FormCard.FormDelegateSeparator {
            visible: buildsAtText.visible
        }

        FormCard.FormTextDelegate {
            id: buildsAtText
            objectName: "buildsAtText"
            visible: pobPage.options.buildsAt !== ""
            text: qsTr("Your builds")
            description: pobPage.options.buildsAt
        }

        FormCard.FormDelegateSeparator {
            visible: stepsText.visible
        }

        FormCard.FormTextDelegate {
            id: stepsText
            objectName: "stepsText"
            visible: pobPage.options.steps.length > 0
            text: qsTr("What to do")
            description: pobPage.options.steps.join("\n")
        }

        FormCard.FormDelegateSeparator {
            visible: discoverButton.visible
        }

        FormCard.FormButtonDelegate {
            id: discoverButton
            objectName: "discoverButton"
            visible: pobPage.options.offersFlatpak
            text: qsTr("Open in Discover")
            description: qsTr("Install it from Discover, the app store on KDE Plasma.")
            icon.name: "plasmadiscover"
            onClicked: pobPage.options.openDiscover()
        }

        FormCard.FormDelegateSeparator {
            visible: flathubButton.visible
        }

        FormCard.FormButtonDelegate {
            id: flathubButton
            objectName: "flathubButton"
            visible: pobPage.options.offersFlatpak
            text: qsTr("Open Flathub in the Browser")
            description: qsTr("Its page on Flathub, for any other app store.")
            icon.name: "internet-web-browser"
            onClicked: pobPage.options.openFlathub()
        }

        FormCard.FormDelegateSeparator {
            visible: copyCommandButton.visible
        }

        FormCard.FormButtonDelegate {
            id: copyCommandButton
            objectName: "copyCommandButton"
            visible: pobPage.options.offersFlatpak
            text: qsTr("Copy the Install Command")
            description: pobPage.options.installCommand
            icon.name: "edit-copy"
            onClicked: pobPage.options.copyInstallCommand()
        }

        FormCard.FormDelegateSeparator {
            visible: installerButton.visible
        }

        FormCard.FormButtonDelegate {
            id: installerButton
            objectName: "installerButton"
            visible: pobPage.options.offersInstaller
            text: qsTr("Open the Download Page")
            description: qsTr("Its releases page on GitHub, with the installer.")
            icon.name: "internet-web-browser"
            onClicked: pobPage.options.openInstaller()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "lookAgainButton"
            text: qsTr("Look Again")
            description: qsTr("Look in the usual places again. Use it after you install Path of Building, "
                              + "or open it for Path of Exile 2 for the first time.")
            icon.name: "view-refresh"
            onClicked: pobPage.app.lookForPob()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextFieldDelegate {
            id: folderField
            objectName: "folderField"
            label: qsTr("Folder")
            placeholderText: qsTr("Leave empty to use the one found")
            // Set from the options by hand, as Kirigami Addons 1.7 drops a
            // binding on this one
            Component.onCompleted: text = pobPage.options.location
            onEditingFinished: pobPage.options.location = text
            Connections {
                target: pobPage.options
                function onChanged() {
                    if (folderField.text !== pobPage.options.location)
                        folderField.text = pobPage.options.location
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            objectName: "folderNote"
            text: pobPage.options.folderNote
            textItem.wrapMode: Text.Wrap
            textItem.color: pobPage.options.folderProblem ? Kirigami.Theme.negativeTextColor
                                                          : Kirigami.Theme.textColor
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "chooseFolderButton"
            text: qsTr("Choose a Folder")
            description: qsTr("The one with HeadlessWrapper.lua in it, or its repository.")
            icon.name: "document-open-folder"
            onClicked: folderDialog.open()
        }

        FormCard.FormDelegateSeparator {
            visible: usualPlacesButton.visible
        }

        FormCard.FormButtonDelegate {
            id: usualPlacesButton
            objectName: "usualPlacesButton"
            visible: pobPage.options.location !== ""
            text: qsTr("Use the Usual Places")
            description: qsTr("Clears the folder, and goes back to the one found.")
            icon.name: "edit-clear"
            onClicked: pobPage.options.useUsualPlaces()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "placesButton"
            text: pobPage.showPlaces ? qsTr("Hide Where It Looks") : qsTr("Show Where It Looks")
            description: qsTr("Every place it looks in, and whether Path of Building is there.")
            icon.name: pobPage.showPlaces ? "arrow-up" : "arrow-down"
            onClicked: pobPage.showPlaces = !pobPage.showPlaces
        }

        FormCard.FormDelegateSeparator {
            visible: placesText.visible
        }

        FormCard.FormTextDelegate {
            id: placesText
            objectName: "placesText"
            visible: pobPage.showPlaces
            text: pobPage.options.places.join("\n")
            textItem.wrapMode: Text.Wrap
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

        SwitchRow {
            objectName: "resistsSwitch"
            text: qsTr("Cap my resistances first")
            description: qsTr("While one is under the cap, items that fill it score much higher.")
            checked: pobPage.options.resistsFirst
            onToggled: pobPage.options.resistsFirst = checked
        }

        FormCard.FormDelegateSeparator {}

        SwitchRow {
            objectName: "chaosSwitch"
            text: qsTr("Count chaos resistance too")
            enabled: pobPage.options.resistsFirst
            checked: pobPage.options.chaos
            onToggled: pobPage.options.chaos = checked
        }

        FormCard.FormDelegateSeparator {}

        SwitchRow {
            objectName: "keepDamageSwitch"
            text: qsTr("Never lose damage")
            description: qsTr("An item that costs a good part of your damage is never an upgrade.")
            checked: pobPage.options.keepDamage
            onToggled: pobPage.options.keepDamage = checked
        }

        FormCard.FormDelegateSeparator {}

        SwitchRow {
            objectName: "keepDefenceSwitch"
            text: qsTr("Never lose defences")
            description: qsTr("An item that costs a good part of your Effective HP, Life or Energy Shield is never an upgrade.")
            checked: pobPage.options.keepDefence
            onToggled: pobPage.options.keepDefence = checked
        }

        FormCard.FormDelegateSeparator {}

        SwitchRow {
            objectName: "spiritSwitch"
            text: qsTr("Never run short of Spirit")
            description: qsTr("For your minions and auras.")
            checked: pobPage.options.spirit
            onToggled: pobPage.options.spirit = checked
        }

        FormCard.FormDelegateSeparator {}

        SwitchRow {
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
