// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The stats the character you play doesn't want counted, a group at a time
// or one by one.
ColumnsPage {
    id: ignoredPage

    required property IgnoredStats ignored
    required property StatPicker picker

    // On a wide window the groups and the stats are tables, as on the weights
    // page.
    readonly property real noteWidth: Kirigami.Units.gridUnit * 20
    readonly property real modeWidth: Kirigami.Units.gridUnit * 12

    title: qsTr("Ignored Stats")
    description: qsTr("Stats your character does not want counted.")
    maxColumns: wide ? 1 : 3
    maxColumnWidth: Kirigami.Units.gridUnit * (wide ? 66 : 40)

    // A group of stats, with how they count beside it.
    component GroupRow: FormRow {
        id: groupRow

        required property ignoredGroup modelData

        objectName: "group:" + modelData.id
        text: modelData.name
        description: modelData.stats
        descriptionWidth: ignoredPage.wide ? ignoredPage.noteWidth : 0
        controlsWidth: ignoredPage.wide ? ignoredPage.modeWidth : 0

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

    // A stat ignored by itself, which can be ignored or excluded, or taken off
    // the list.
    component StatRow: FormRow {
        id: statRow

        required property string key
        required property string name
        required property string note
        required property int mode

        objectName: "ignored:" + key
        text: name
        description: note
        descriptionWidth: ignoredPage.wide ? ignoredPage.noteWidth : 0
        controlsWidth: ignoredPage.wide ? ignoredPage.modeWidth : 0

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

    SectionHeader {
        title: ignoredPage.ignored.character !== "" ? qsTr("For %1").arg(ignoredPage.ignored.character) : qsTr("Ignored Stats")
    }

    Sheet {
        FormCard.FormTextDelegate {
            objectName: "nobodyText"
            visible: ignoredPage.ignored.character === ""
            text: qsTr("You play nobody yet.")
            description: qsTr("A character is made for you when you change something here. Or add one, or import a build, on the Characters page.")
        }

        TableHeader {
            objectName: "groupsHeader"
            visible: ignoredPage.wide
            first: qsTr("Group")
            second: qsTr("Stats in it")
            third: qsTr("Counts as")
            descriptionWidth: ignoredPage.noteWidth
            controlsWidth: ignoredPage.modeWidth
        }

        RowRule {
            visible: ignoredPage.wide
        }

        Repeater {
            model: ignoredPage.ignored.groups

            delegate: GroupRow {}
        }

        RowRule {
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

    SectionText {
        objectName: "ignoredHelp"
        text: qsTr("An ignored stat counts for nothing in the gear score and your weights, and the trade searches leave it out. An excluded one is ignored too, and the trade searches skip every listing that has it.")
    }

    SectionHeader {
        visible: singleStats.visible
        title: qsTr("One by One")
    }

    Sheet {
        id: singleStats
        objectName: "singleStats"

        TableHeader {
            objectName: "singleHeader"
            visible: ignoredPage.wide && statRows.count > 0
            first: qsTr("Stat")
            second: qsTr("Note")
            third: qsTr("Counts as")
            descriptionWidth: ignoredPage.noteWidth
            controlsWidth: ignoredPage.modeWidth
        }

        RowRule {
            visible: ignoredPage.wide && statRows.count > 0
        }

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

        RowRule {}

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
