// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The setup guide's trade searches: whether to make them, and the session ID
// to sign in with, which is optional.
ColumnLayout {
    id: step

    required property TradeOptions options
    required property TradeSession session

    spacing: 0

    Sheet {
        SwitchRow {
            objectName: "setupTradeSwitch"
            text: qsTr("Search the trade site")
            description: qsTr("From the popup, for upgrades and for items like the one you checked.")
            checked: step.options.enabled
            onToggled: step.options.enabled = checked
        }
    }

    FormCard.FormHeader {
        title: qsTr("Signed In")
    }

    Sheet {
        FormCard.FormTextDelegate {
            objectName: "setupSessionHelp"
            text: qsTr("You can leave this for later. Signed in, the site takes bigger searches. The session ID is the POESESSID cookie of a browser that's signed in to pathofexile.com, and it's kept in your system keyring.")
            textItem.wrapMode: Text.Wrap
        }

        RowRule {}

        FormRow {
            text: qsTr("Session ID")
            enabled: !step.session.busy

            QQC2.TextField {
                id: sessionField
                objectName: "setupSessionField"
                Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Paste it here")
                Accessible.name: qsTr("Session ID")
                onAccepted: saveButton.clicked()

                // it isn't kept here once it's saved
                Connections {
                    target: step.session

                    function onStored() {
                        sessionField.clear();
                    }
                }
            }

            QQC2.Button {
                id: saveButton
                objectName: "setupSaveButton"
                text: qsTr("Save")
                enabled: sessionField.text.trim() !== ""
                onClicked: step.session.save(sessionField.text)
            }
        }

        RowRule {}

        FormCard.FormTextDelegate {
            objectName: "setupSessionStatus"
            text: step.session.status
            textItem.wrapMode: Text.Wrap
        }

        RowRule {
            visible: sessionError.visible
        }

        FormCard.FormTextDelegate {
            id: sessionError
            objectName: "setupSessionError"
            visible: text !== ""
            text: step.session.error
            textItem.wrapMode: Text.Wrap
            textItem.color: Kirigami.Theme.negativeTextColor
        }
    }
}
