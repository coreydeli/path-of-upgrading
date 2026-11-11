// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What an item costs you or puts at risk, a line each, with a diamond in the
// color of its tone in place of a message box.
ColumnLayout {
    id: notice

    property list<string> lines
    property int tone: Tone.Warning
    property bool colorblind: false

    readonly property color tint: Style.tone(tone, colorblind, Kirigami.Theme)

    spacing: Kirigami.Units.smallSpacing
    visible: lines.length > 0

    Repeater {
        model: notice.lines

        RowLayout {
            id: line

            required property string modelData

            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing * 2

            // bigger for a loss than for a risk
            Rectangle {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 5
                implicitWidth: notice.tone === Tone.Bad ? 9 : 7
                implicitHeight: implicitWidth
                rotation: 45
                antialiasing: true
                color: notice.tint
            }

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.italic: true
                color: notice.tint
                text: line.modelData
            }
        }
    }
}
