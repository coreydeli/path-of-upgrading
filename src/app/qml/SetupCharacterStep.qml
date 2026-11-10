// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The setup guide's character: add one, or import a build for one. The
// same things as Import Character does.
ColumnLayout {
    id: step

    required property CharacterList characters
    required property BuildImporter importer

    // for builds you've saved in Path of Building since
    onVisibleChanged: {
        if (visible)
            importer.refreshBuilds();
    }

    spacing: 0

    FormCard.FormHeader {
        visible: characterRows.count > 0
        title: qsTr("Your Characters")
    }

    FormCard.FormCard {
        visible: characterRows.count > 0

        Repeater {
            id: characterRows
            model: step.characters

            delegate: FormCard.FormTextDelegate {
                required property string name
                required property string build

                objectName: "setupCharacter:" + name
                text: name
                description: build !== "" ? build : qsTr("No build imported")
            }
        }
    }

    FormCard.FormHeader {
        title: qsTr("Add a Character")
    }

    FormCard.FormCard {
        FormCard.FormTextFieldDelegate {
            id: nameField
            objectName: "setupNameField"
            label: qsTr("Name")
            placeholderText: qsTr("As it's called in the game")
            onAccepted: addButton.clicked()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            id: addButton
            objectName: "setupAddButton"
            text: qsTr("Add")
            icon.name: "list-add"
            enabled: nameField.text.trim() !== ""
            onClicked: {
                if (step.characters.add(nameField.text) !== "")
                    nameField.text = "";
            }
        }

        FormCard.FormDelegateSeparator {
            visible: charactersError.visible
        }

        FormCard.FormTextDelegate {
            id: charactersError
            objectName: "setupCharactersError"
            visible: text !== ""
            text: step.characters.error
            textItem.wrapMode: Text.Wrap
            textItem.color: Kirigami.Theme.negativeTextColor
        }
    }

    FormCard.FormHeader {
        title: qsTr("Import a Build")
    }

    FormCard.FormCard {
        FormCard.FormTextFieldDelegate {
            id: codeField
            objectName: "setupCodeField"
            label: qsTr("Code or link")
            placeholderText: qsTr("From Path of Building or pobb.in")
            onAccepted: importButton.clicked()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            id: importButton
            objectName: "setupImportButton"
            text: step.importer.busy ? qsTr("Downloading the Build") : qsTr("Import")
            icon.name: "document-import"
            enabled: !step.importer.busy
            onClicked: step.importer.importText(codeField.text)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "setupFileButton"
            text: qsTr("From a File")
            icon.name: "document-open"
            enabled: !step.importer.busy
            onClicked: fileDialog.open()
        }

        Repeater {
            model: step.importer.builds

            delegate: FormCard.FormButtonDelegate {
                required property var modelData

                objectName: "setupSaved:" + modelData.name
                text: modelData.name
                description: qsTr("Saved in Path of Building")
                icon.name: "document-import"
                enabled: !step.importer.busy
                onClicked: step.importer.importFile(modelData.path)
            }
        }

        FormCard.FormDelegateSeparator {
            visible: importMessage.visible
        }

        FormCard.FormTextDelegate {
            id: importMessage
            objectName: "setupImportMessage"
            visible: text !== ""
            text: step.importer.message
            textItem.wrapMode: Text.Wrap
            textItem.color: step.importer.failed ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
        }
    }

    // a code that's imported needn't be pasted again
    Connections {
        target: step.importer

        function onMessageChanged() {
            if (step.importer.message !== "" && !step.importer.failed)
                codeField.text = "";
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Import a Build")
        nameFilters: [qsTr("Path of Building builds (*.xml)"), qsTr("All files (*)")]
        onAccepted: step.importer.importFileUrl(selectedFile)
    }
}
