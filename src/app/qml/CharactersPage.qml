pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Your characters, the one you play, and the Path of Building builds you
// import for them.
FormCard.FormCardPage {
    id: charactersPage

    required property App app
    readonly property CharacterList characters: app.characterList
    readonly property BuildImporter importer: app.buildImporter

    title: qsTr("Characters")

    // for builds you've saved in Path of Building since
    onIsCurrentPageChanged: {
        if (isCurrentPage)
            importer.refreshBuilds();
    }

    FormCard.FormHeader {
        title: qsTr("Your Characters")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            objectName: "noCharactersText"
            visible: characterRows.count === 0
            text: qsTr("You have none yet.")
            description: qsTr("Add one, or import a build and one is made for it.")
        }

        Repeater {
            id: characterRows
            model: charactersPage.characters

            delegate: FormCard.FormRadioDelegate {
                id: characterRow

                required property string characterId
                required property string name
                required property string build

                objectName: "character:" + name
                text: name
                description: build !== "" ? build : qsTr("No build imported")
                // the list says which it is, so it stays right when that
                // changes some other way
                checkable: false
                checked: characterId === charactersPage.characters.current
                onClicked: charactersPage.characters.current = characterId

                trailing: RowLayout {
                    spacing: 0

                    QQC2.ToolButton {
                        objectName: "renameButton"
                        text: qsTr("Rename")
                        icon.name: "edit-rename"
                        display: QQC2.AbstractButton.IconOnly
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: renameDialog.ask(characterRow.characterId, characterRow.name)
                    }

                    QQC2.ToolButton {
                        objectName: "removeButton"
                        text: qsTr("Remove")
                        icon.name: "edit-delete"
                        display: QQC2.AbstractButton.IconOnly
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: removeDialog.ask(characterRow.characterId, characterRow.name)
                    }
                }
            }
        }

        FormCard.FormDelegateSeparator {
            visible: charactersError.visible
        }

        FormCard.FormTextDelegate {
            id: charactersError
            objectName: "charactersError"
            visible: text !== ""
            text: charactersPage.characters.error
            textItem.wrapMode: Text.Wrap
            textItem.color: Kirigami.Theme.negativeTextColor
        }

        FormCard.FormDelegateSeparator {
            visible: problemsText.visible
        }

        FormCard.FormTextDelegate {
            id: problemsText
            objectName: "problemsText"
            visible: charactersPage.characters.problems.length > 0
            text: qsTr("Some characters couldn't be read, and were left as they are:")
            description: charactersPage.characters.problems.join("\n")
            textItem.wrapMode: Text.Wrap
            descriptionItem.wrapMode: Text.Wrap
        }
    }

    FormCard.FormHeader {
        title: qsTr("Add a Character")
    }

    FormCard.FormCard {
        FormCard.FormTextFieldDelegate {
            id: nameField
            objectName: "nameField"
            label: qsTr("Name")
            placeholderText: qsTr("As it's called in the game")
            onAccepted: addButton.clicked()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            id: addButton
            objectName: "addButton"
            text: qsTr("Add")
            description: qsTr("You then play it.")
            icon.name: "list-add"
            enabled: nameField.text.trim() !== ""
            onClicked: {
                if (charactersPage.characters.add(nameField.text) !== "")
                    nameField.text = "";
            }
        }
    }

    FormCard.FormHeader {
        title: qsTr("Import a Build")
    }

    FormCard.FormCard {
        FormCard.FormTextFieldDelegate {
            id: codeField
            objectName: "codeField"
            label: qsTr("Code or link")
            placeholderText: qsTr("From Path of Building, pobb.in or poe.ninja")
            onAccepted: importButton.clicked()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            id: importButton
            objectName: "importButton"
            text: charactersPage.importer.busy ? qsTr("Downloading the Build") : qsTr("Import")
            description: qsTr("Its gear goes on the character you play, or on a new one when you have none. Items you copied in the game stay, as they say more.")
            icon.name: "document-import"
            enabled: !charactersPage.importer.busy
            onClicked: charactersPage.importer.importText(codeField.text)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "fileButton"
            text: qsTr("From a File")
            icon.name: "document-open"
            enabled: !charactersPage.importer.busy
            onClicked: fileDialog.open()
        }

        FormCard.FormDelegateSeparator {
            visible: importMessage.visible
        }

        FormCard.FormTextDelegate {
            id: importMessage
            objectName: "importMessage"
            visible: text !== ""
            text: charactersPage.importer.message
            textItem.wrapMode: Text.Wrap
            textItem.color: charactersPage.importer.failed ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
        }
    }

    FormCard.FormHeader {
        visible: savedBuilds.visible
        title: qsTr("Saved in Path of Building")
    }

    FormCard.FormCard {
        id: savedBuilds
        objectName: "savedBuilds"
        visible: charactersPage.importer.buildsFolder !== ""

        FormCard.FormTextDelegate {
            objectName: "noSavedBuildsText"
            visible: savedRows.count === 0
            text: qsTr("There are none in %1 yet.").arg(charactersPage.importer.buildsFolder)
            textItem.wrapMode: Text.Wrap
        }

        Repeater {
            id: savedRows
            model: charactersPage.importer.builds

            delegate: FormCard.FormButtonDelegate {
                required property var modelData

                objectName: "saved:" + modelData.name
                text: modelData.name
                description: qsTr("Saved %1").arg(modelData.modified.toLocaleString(Qt.locale(), Locale.ShortFormat))
                icon.name: "document-import"
                enabled: !charactersPage.importer.busy
                onClicked: charactersPage.importer.importFile(modelData.path)
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "refreshBuildsButton"
            text: qsTr("Look Again")
            description: qsTr("For a build you've saved since.")
            icon.name: "view-refresh"
            onClicked: charactersPage.importer.refreshBuilds()
        }
    }

    // a code that's imported needn't be pasted again
    Connections {
        target: charactersPage.importer

        function onMessageChanged() {
            if (charactersPage.importer.message !== "" && !charactersPage.importer.failed)
                codeField.text = "";
        }
    }

    FormCard.FormCardDialog {
        id: renameDialog
        objectName: "renameDialog"

        property string characterId

        function ask(id: string, name: string) {
            characterId = id;
            renameField.text = name;
            open();
            renameField.forceActiveFocus();
            renameField.selectAll();
        }

        title: qsTr("Rename")
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        onAccepted: charactersPage.characters.rename(characterId, renameField.text)

        Component.onCompleted: {
            const rename = standardButton(QQC2.Dialog.Ok);
            rename.text = qsTr("Rename");
            rename.icon.name = "edit-rename";
            rename.enabled = Qt.binding(() => renameField.text.trim() !== "");
        }

        FormCard.FormTextFieldDelegate {
            id: renameField
            objectName: "renameField"
            label: qsTr("Name")
            onAccepted: {
                if (text.trim() !== "")
                    renameDialog.accept();
            }
        }
    }

    FormCard.FormCardDialog {
        id: removeDialog
        objectName: "removeDialog"

        property string characterId
        property string name

        function ask(id: string, name: string) {
            characterId = id;
            removeDialog.name = name;
            open();
        }

        title: qsTr("Remove %1?").arg(name)
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        onAccepted: charactersPage.characters.remove(characterId)

        Component.onCompleted: {
            const remove = standardButton(QQC2.Dialog.Ok);
            remove.text = qsTr("Remove");
            remove.icon.name = "edit-delete";
        }

        FormCard.FormTextDelegate {
            text: qsTr("Its gear, weights and imported build go with it.")
            textItem.wrapMode: Text.Wrap
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Import a Build")
        nameFilters: [qsTr("Path of Building builds (*.xml)"), qsTr("All files (*)")]
        onAccepted: charactersPage.importer.importFileUrl(selectedFile)
    }
}
