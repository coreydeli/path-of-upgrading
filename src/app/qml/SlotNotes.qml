// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What would raise a slot's score, or what an empty slot wants: a labelled
// line each, with the stats beside the label as chips. A note with no stats
// is a sentence, which wraps.
ColumnLayout {
    id: notes

    // gearNotes
    property var lines: []

    spacing: Kirigami.Units.largeSpacing

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
                // level with the text in the first chip
                topPadding: Kirigami.Units.smallSpacing
                text: note.modelData.label
                color: Kirigami.Theme.disabledTextColor
            }

            StatChips {
                objectName: "noteText"
                Layout.fillWidth: true
                visible: note.modelData.stats.length > 0
                stats: note.modelData.stats
                text: note.modelData.text
            }

            QQC2.Label {
                objectName: "noteSentence"
                Layout.fillWidth: true
                visible: note.modelData.stats.length === 0
                topPadding: Kirigami.Units.smallSpacing
                text: note.modelData.text
                wrapMode: Text.Wrap
            }
        }
    }
}
