// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// A slot on the gear page: what's in it, what would raise its score, and
// the score. Wide, the three sit side by side. Narrow, the notes go under
// the other two. It lays them out itself, as a FormTextDelegate's trailing
// item stays hidden for good once its page or card has been hidden.
FormCard.AbstractFormDelegate {
    id: row

    required property string slot
    required property string name
    required property string base
    required property bool fromPob
    required property string score
    required property string grade
    // the ways the item falls short, or what an empty slot wants: { label, text }
    required property var notes
    // the first of its card, which has no line above it
    required property bool first

    // what's in it in one line, for the screen reader and the tests
    readonly property string description: {
        if (name === "")
            return qsTr("Nothing saved");
        const parts = [name];
        if (base !== "")
            parts.push(base);
        if (fromPob)
            parts.push(qsTr("from Path of Building"));
        return parts.join(", ");
    }
    readonly property bool wide: width >= Kirigami.Units.gridUnit * 38

    signal takeOffAsked

    objectName: "slot:" + slot
    text: slot
    focusPolicy: Qt.NoFocus
    background: null
    topPadding: Kirigami.Units.largeSpacing
    bottomPadding: Kirigami.Units.largeSpacing
    Accessible.name: slot
    Accessible.description: description

    Kirigami.Separator {
        visible: !row.first
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            leftMargin: Kirigami.Units.largeSpacing
            rightMargin: Kirigami.Units.largeSpacing
        }
    }

    contentItem: GridLayout {
        columns: row.wide ? 3 : 2
        columnSpacing: Kirigami.Units.gridUnit
        rowSpacing: Kirigami.Units.smallSpacing

        // the slot, then the item in it
        ColumnLayout {
            Layout.row: 0
            Layout.column: 0
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: !row.wide
            Layout.preferredWidth: row.wide ? Kirigami.Units.gridUnit * 14 : -1
            Layout.maximumWidth: row.wide ? Kirigami.Units.gridUnit * 14 : -1
            spacing: 0

            QQC2.Label {
                objectName: "slotName"
                Layout.fillWidth: true
                text: row.slot
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Accessible.ignored: true
            }

            QQC2.Label {
                objectName: "itemName"
                Layout.fillWidth: true
                text: row.name !== "" ? row.name : qsTr("Nothing saved")
                color: row.name !== "" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }

            QQC2.Label {
                objectName: "itemBase"
                Layout.fillWidth: true
                visible: text !== ""
                text: {
                    const parts = [];
                    if (row.name !== "" && row.base !== "")
                        parts.push(row.base);
                    if (row.name !== "" && row.fromPob)
                        parts.push(qsTr("from Path of Building"));
                    return parts.join(", ");
                }
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }
        }

        // what would raise the score
        SlotNotes {
            objectName: "slotNotes"
            Layout.row: row.wide ? 0 : 1
            Layout.column: row.wide ? 1 : 0
            Layout.columnSpan: row.wide ? 1 : 2
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            visible: row.notes.length > 0
            lines: row.notes
        }

        // the score and taking it off
        RowLayout {
            Layout.row: 0
            Layout.column: row.wide ? 2 : 1
            Layout.alignment: Qt.AlignTop | Qt.AlignRight
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                objectName: "gearScore"
                Layout.alignment: Qt.AlignVCenter
                visible: row.score !== ""
                text: qsTr("Gear score %1 %2").arg(row.score).arg(row.grade)
                color: Style.grade(row.grade, Kirigami.Theme)
                font.weight: Font.DemiBold
            }

            QQC2.ToolButton {
                objectName: "takeOffButton"
                visible: row.name !== ""
                text: qsTr("Take Off")
                icon.name: "list-remove"
                display: QQC2.AbstractButton.IconOnly
                Accessible.name: qsTr("Take Off %1").arg(row.slot)
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: row.takeOffAsked()
            }
        }
    }
}
