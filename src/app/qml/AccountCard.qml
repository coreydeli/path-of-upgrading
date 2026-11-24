// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Linking your Path of Exile account, and importing one of its characters
// from the list. The page shows it only when the build can link at all.
Sheet {
    id: card
    objectName: "accountCard"

    required property PoeAccount account
    required property AccountCharacters characters

    FormRow {
        objectName: "accountStatus"
        text: qsTr("Your account")
        description: card.account.status

        QQC2.Button {
            objectName: "linkButton"
            visible: !card.account.linked && !card.account.waiting
            highlighted: true
            text: qsTr("Link Your Account")
            enabled: !card.account.busy
            onClicked: card.account.link()
        }

        QQC2.Button {
            objectName: "cancelLinkButton"
            visible: card.account.waiting
            text: qsTr("Cancel")
            onClicked: card.account.cancel()
        }

        QQC2.Button {
            objectName: "unlinkButton"
            visible: card.account.linked
            text: qsTr("Unlink")
            onClicked: card.account.unlink()
        }
    }

    RowRule {
        visible: accountError.visible
    }

    FormCard.FormTextDelegate {
        id: accountError
        objectName: "accountError"
        visible: text !== ""
        text: card.account.error
        textItem.wrapMode: Text.Wrap
        textItem.color: Kirigami.Theme.negativeTextColor
    }

    RowRule {
        visible: accountNotes.visible
    }

    FormCard.FormTextDelegate {
        id: accountNotes
        objectName: "accountNotes"
        visible: !card.account.linked
        text: card.account.notes.join("\n")
        textItem.wrapMode: Text.Wrap
    }

    RowRule {
        visible: card.account.linked
    }

    FormRow {
        objectName: "accountCharactersRow"
        visible: card.account.linked
        text: qsTr("Characters on your account")
        description: card.characters.loading ? qsTr("Asking the website") : qsTr("Pick one to import. Its gear goes on the character linked to it, or on a new one.")

        QQC2.Button {
            objectName: "refreshAccountButton"
            text: qsTr("Refresh")
            enabled: !card.characters.loading && !card.characters.importing
            onClicked: card.characters.refresh()
        }

        QQC2.Button {
            objectName: "revokeButton"
            text: qsTr("Revoke Access")
            QQC2.ToolTip.text: qsTr("Opens your account's applications page on the website, where the link can be taken away.")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: card.account.openRevokePage()
        }
    }

    RowRule {
        visible: charactersError.visible
    }

    FormCard.FormTextDelegate {
        id: charactersError
        objectName: "accountCharactersError"
        visible: card.account.linked && text !== ""
        text: card.characters.error
        textItem.wrapMode: Text.Wrap
        textItem.color: Kirigami.Theme.negativeTextColor
    }

    FormCard.FormTextDelegate {
        objectName: "noAccountCharactersText"
        visible: card.account.linked && card.characters.loaded && accountRows.count === 0
        text: qsTr("There are no characters on this account yet.")
    }

    Repeater {
        id: accountRows
        model: card.account.linked ? card.characters : null

        delegate: FormRow {
            id: accountRow

            required property int index
            required property string name
            required property string summary
            required property bool imported

            objectName: "account:" + name
            text: name
            description: summary

            QQC2.Button {
                objectName: "importAccountButton"
                text: accountRow.imported ? qsTr("Update From the Game") : qsTr("Import")
                enabled: !card.characters.importing && !card.characters.loading
                onClicked: card.characters.importCharacter(accountRow.index)
            }
        }
    }

    RowRule {
        visible: importMessage.visible
    }

    FormCard.FormTextDelegate {
        id: importMessage
        objectName: "accountImportMessage"
        visible: card.account.linked && text !== ""
        text: card.characters.importing ? qsTr("Fetching the character from the website.") : card.characters.message
        textItem.wrapMode: Text.Wrap
    }

    RowRule {}

    FormCard.FormTextDelegate {
        objectName: "accountDisclaimer"
        text: card.account.disclaimer
        textItem.wrapMode: Text.Wrap
        textItem.color: Kirigami.Theme.disabledTextColor
        textItem.font: Kirigami.Theme.smallFont
    }
}
