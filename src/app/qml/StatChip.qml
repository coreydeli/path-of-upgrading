// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// One stat in a box of its own, so a list of them reads one at a time. In
// Dossier it's a small plate with its corners cut, square when the
// decorations are plain, and in Material an outlined chip.
Item {
    id: chip

    property string text
    // what it may take up before its text wraps
    property real maxWidth: Infinity

    readonly property real across: Kirigami.Units.smallSpacing * 2
    readonly property real down: Kirigami.Units.smallSpacing

    implicitWidth: label.implicitWidth + 2 * across
    implicitHeight: label.implicitHeight + 2 * down
    width: Math.min(implicitWidth, maxWidth)
    height: label.height + 2 * down

    Plate {
        anchors.fill: parent
        fill: Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.06)
        line: Style.inner(Kirigami.Theme)
        notch: Style.plain ? 0 : Kirigami.Units.smallSpacing
        radius: Style.shapeSmall
    }

    QQC2.Label {
        id: label

        x: chip.across
        y: chip.down
        width: Math.max(0, chip.width - 2 * chip.across)
        text: chip.text
        wrapMode: Text.Wrap
    }
}
