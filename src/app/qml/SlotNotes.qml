// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What would raise a slot's score, or what an empty slot wants: a labelled
// line each, with the stats beside the label as chips.
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

            NoteLabel {
                objectName: "noteLabel"
                text: note.modelData.label
                help: note.modelData.help
            }

            StatChips {
                objectName: "noteText"
                Layout.fillWidth: true
                visible: note.modelData.stats.length > 0
                stats: note.modelData.stats
                text: note.modelData.text
            }
        }
    }
}
