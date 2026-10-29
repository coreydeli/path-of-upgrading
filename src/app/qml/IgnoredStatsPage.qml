pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The stats the character you play doesn't want counted, a group at a time
// or one by one.
FormCard.FormCardPage {
    id: ignoredPage

    required property IgnoredStats ignored
    required property StatPicker picker

    title: qsTr("Ignored Stats")

    // A group of stats, with how they count beside it.
    component GroupRow: FormRow {
        id: groupRow

        required property ignoredGroup modelData

        objectName: "group:" + modelData.id
        text: modelData.name
        description: modelData.stats

        QQC2.ComboBox {
            objectName: "modeBox"
            // all as wide, for them to line up
            implicitContentWidthPolicy: QQC2.ComboBox.WidestText
            model: ignoredPage.ignored.modes
            currentIndex: groupRow.modelData.mode
            Accessible.name: groupRow.modelData.name
            onActivated: chosen => ignoredPage.ignored.setGroupMode(groupRow.modelData.id, chosen)
        }
    }

    // A stat ignored by itself, which can't be counted again but removed.
    component StatRow: FormRow {
        id: statRow

        required property string key
        required property string name
        required property string note
        required property int mode

        objectName: "ignored:" + key
        text: name
        description: note

        QQC2.ComboBox {
            objectName: "modeBox"
            implicitContentWidthPolicy: QQC2.ComboBox.WidestText
            model: ignoredPage.ignored.modes.slice(IgnoredStats.Ignored)
            currentIndex: statRow.mode - IgnoredStats.Ignored
            Accessible.name: statRow.name
            onActivated: chosen => ignoredPage.ignored.setMode(statRow.key, chosen + IgnoredStats.Ignored)
        }

        QQC2.ToolButton {
            objectName: "removeButton"
            text: qsTr("Remove")
            icon.name: "list-remove"
            display: QQC2.AbstractButton.IconOnly
            Accessible.name: qsTr("Remove %1").arg(statRow.name)
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: ignoredPage.ignored.remove(statRow.key)
        }
    }

    FormCard.FormHeader {
        title: ignoredPage.ignored.character !== "" ? qsTr("For %1").arg(ignoredPage.ignored.character) : qsTr("Ignored Stats")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            objectName: "nobodyText"
            visible: ignoredPage.ignored.character === ""
            text: qsTr("You play nobody yet.")
            description: qsTr("Add a character or import a build on the Characters page.")
        }

        Repeater {
            model: ignoredPage.ignored.groups

            delegate: GroupRow {}
        }

        FormCard.FormDelegateSeparator {
            visible: ignoredError.visible
        }

        FormCard.FormTextDelegate {
            id: ignoredError
            objectName: "ignoredError"
            visible: text !== ""
            text: ignoredPage.ignored.error
            textItem.wrapMode: Text.Wrap
            textItem.color: Kirigami.Theme.negativeTextColor
        }
    }

    FormCard.FormSectionText {
        objectName: "ignoredHelp"
        visible: ignoredPage.ignored.character !== ""
        text: qsTr("An ignored stat counts for nothing in the gear score and your weights, and the trade searches leave it out. An excluded one is ignored too, and the trade searches skip every listing that has it.")
    }

    FormCard.FormHeader {
        visible: singleStats.visible
        title: qsTr("One by One")
    }

    FormCard.FormCard {
        id: singleStats
        objectName: "singleStats"
        visible: ignoredPage.ignored.character !== ""

        Repeater {
            id: statRows
            model: ignoredPage.ignored

            delegate: StatRow {}
        }

        FormCard.FormTextDelegate {
            objectName: "noSingleText"
            visible: statRows.count === 0
            text: qsTr("You ignore no stat by itself.")
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "ignoreStatButton"
            text: qsTr("Ignore a Stat")
            icon.name: "list-add"
            onClicked: statPicker.open()
        }
    }

    StatPickerDialog {
        id: statPicker

        objectName: "ignorePicker"
        title: qsTr("Ignore a Stat")
        picker: ignoredPage.picker
        taken: ignoredPage.ignored.keys
        takenNote: qsTr("You ignore it already")
        onPicked: key => ignoredPage.ignored.add(key)
    }
}
