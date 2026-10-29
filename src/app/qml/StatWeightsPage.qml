pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// What each stat is worth to the character you play, for judging items
// without a build profile and searching the trade site by your weights.
FormCard.FormCardPage {
    id: weightsPage

    required property StatWeights weights
    required property StatPicker picker

    title: qsTr("Stat Weights")

    // A stat and its weight.
    component WeightRow: FormRow {
        id: weightRow

        required property string key
        required property string name
        required property string note
        required property real weight

        objectName: "weight:" + key
        text: name
        description: note

        NumberBox {
            objectName: "weightBox"
            minimum: -1000
            maximum: 1000
            step: 0.1
            decimals: 3
            number: weightRow.weight
            Accessible.name: weightRow.name
            onEdited: number => weightsPage.weights.setWeight(weightRow.key, number)
        }

        QQC2.ToolButton {
            objectName: "removeButton"
            text: qsTr("Remove")
            icon.name: "list-remove"
            display: QQC2.AbstractButton.IconOnly
            Accessible.name: qsTr("Remove %1").arg(weightRow.name)
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: weightsPage.weights.remove(weightRow.key)
        }
    }

    header: Kirigami.InlineMessage {
        objectName: "undoMessage"
        position: Kirigami.InlineMessage.Position.Header
        visible: weightsPage.weights.undoable !== ""
        text: weightsPage.weights.undoable
        actions: Kirigami.Action {
            objectName: "undoAction"
            text: qsTr("Undo")
            icon.name: "edit-undo"
            onTriggered: weightsPage.weights.undo()
        }
    }

    FormCard.FormHeader {
        title: weightsPage.weights.character !== "" ? qsTr("For %1").arg(weightsPage.weights.character) : qsTr("Stat Weights")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            objectName: "nobodyText"
            visible: weightsPage.weights.character === ""
            text: qsTr("You play nobody yet.")
            description: qsTr("Add a character or import a build on the Characters page.")
        }

        Repeater {
            id: weightRows
            model: weightsPage.weights

            delegate: WeightRow {}
        }

        FormCard.FormTextDelegate {
            objectName: "noWeightsText"
            visible: weightsPage.weights.character !== "" && weightRows.count === 0
            text: qsTr("You have no weights.")
            description: qsTr("Every item comes to 0 by them. Add some, suggest some from your gear, or go back to the defaults.")
            textItem.wrapMode: Text.Wrap
        }

        FormCard.FormDelegateSeparator {
            visible: weightsPage.weights.character !== ""
        }

        FormCard.FormButtonDelegate {
            objectName: "addStatButton"
            visible: weightsPage.weights.character !== ""
            text: qsTr("Add a Stat")
            icon.name: "list-add"
            onClicked: statPicker.open()
        }

        FormCard.FormButtonDelegate {
            objectName: "suggestButton"
            visible: weightsPage.weights.character !== ""
            enabled: weightsPage.weights.suggestions.length > 0
            text: qsTr("Suggest from Your Gear")
            description: enabled ? "" : qsTr("Your gear has no stats to suggest weights for.")
            icon.name: "tools-wizard"
            onClicked: suggestDialog.open()
        }

        FormCard.FormButtonDelegate {
            objectName: "resetButton"
            visible: weightsPage.weights.character !== ""
            enabled: !weightsPage.weights.defaults
            text: qsTr("Back to the Defaults")
            icon.name: "edit-reset"
            onClicked: weightsPage.weights.reset()
        }

        FormCard.FormDelegateSeparator {
            visible: weightsError.visible
        }

        FormCard.FormTextDelegate {
            id: weightsError
            objectName: "weightsError"
            visible: text !== ""
            text: weightsPage.weights.error
            textItem.wrapMode: Text.Wrap
            textItem.color: Kirigami.Theme.negativeTextColor
        }
    }

    FormCard.FormSectionText {
        objectName: "weightsHelp"
        visible: weightsPage.weights.character !== ""
        text: qsTr("A weight is what one of a stat is worth to you, in points. With Life at 1 and Fire Resistance at 1.5, 10% fire resistance counts as much as 15 life. The popup judges items by them when the gear score has no build profile, and so does searching the trade site by your weights.")
    }

    StatPickerDialog {
        id: statPicker

        objectName: "statPicker"
        picker: weightsPage.picker
        taken: weightsPage.weights.keys
        takenNote: qsTr("It has a weight already")
        onPicked: key => weightsPage.weights.add(key)
    }

    FormCard.FormCardDialog {
        id: suggestDialog
        objectName: "suggestDialog"

        // 1.5 rather than 1.500, as in the spin boxes
        function shown(weight: real): string {
            const text = Number(weight).toLocaleString(Qt.locale(), "f", 3);
            return text.replace(new RegExp("\\" + Qt.locale().decimalPoint + "?0+$"), "");
        }

        width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 30)
        title: qsTr("Suggested Weights")
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Apply | QQC2.Dialog.Cancel
        onAccepted: weightsPage.weights.useSuggestions()
        onApplied: {
            weightsPage.weights.addSuggestions();
            close();
        }

        Component.onCompleted: {
            const use = standardButton(QQC2.Dialog.Ok);
            use.text = qsTr("Use These");
            use.icon.name = "dialog-ok-apply";
            const add = standardButton(QQC2.Dialog.Apply);
            add.text = qsTr("Add the Missing Ones");
            add.icon.name = "list-add";
            add.enabled = Qt.binding(() => weightsPage.weights.suggestsNew);
        }

        FormCard.FormTextDelegate {
            text: qsTr("From the stats on your gear, leaving out uniques and runes. A typical roll of each is worth about 30 points, and more when several of your items have it.")
            textItem.wrapMode: Text.Wrap
        }

        FormCard.FormDelegateSeparator {}

        QQC2.ScrollView {
            id: suggestionsView

            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(suggestionRows.implicitHeight, suggestDialog.parent.height * 0.5)
            QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

            ColumnLayout {
                id: suggestionRows

                width: suggestionsView.availableWidth
                spacing: 0

                Repeater {
                    model: weightsPage.weights.suggestions

                    delegate: FormRow {
                        id: suggestionRow

                        required property weightSuggestion modelData

                        objectName: "suggestion:" + modelData.key
                        Layout.fillWidth: true
                        text: modelData.name
                        description: modelData.why

                        QQC2.Label {
                            objectName: "suggestedWeight"
                            text: suggestDialog.shown(suggestionRow.modelData.weight)
                        }
                    }
                }
            }
        }
    }
}
