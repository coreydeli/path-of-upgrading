// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What would raise a slot's score, or what an empty slot wants: a labelled
// line each, with the stats beside the label.
ColumnLayout {
    id: notes

    // { label, text } each
    property var lines: []

    spacing: Kirigami.Units.smallSpacing

    Repeater {
        model: notes.lines

        delegate: RowLayout {
            id: note

            required property var modelData

            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                objectName: "noteLabel"
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                text: note.modelData.label
                color: Kirigami.Theme.disabledTextColor
            }

            QQC2.Label {
                objectName: "noteText"
                Layout.fillWidth: true
                text: note.modelData.text
                wrapMode: Text.Wrap
            }
        }
    }
}
