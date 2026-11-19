// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// One stat in a box of its own, so a list of them reads one at a time. In
// Dossier it's a small plate with its corners cut, square when the
// decorations are plain, in Material an outlined chip, and in Slate a plain
// row with a square tick and a hairline under it, in Pocket a round pill, in Edge a box with a colored edge, in Gilt a box with a gold hairline, and in Cinder a square box with
// uppercase text.
Item {
    id: chip

    property string text
    // "Prefix" or "Suffix", empty when it isn't known
    property string kind
    // what it may take up before its text wraps
    property real maxWidth: Infinity

    readonly property bool tick: Style.chip === "tick"
    // room for the tick
    readonly property real lead: tick ? Kirigami.Units.smallSpacing * 3 : Style.chip === "edge" ? 3 : 0
    readonly property real across: Kirigami.Units.smallSpacing * (Style.chip === "pill" ? 3 : 2)
    readonly property real down: Kirigami.Units.smallSpacing
    readonly property string escaped: text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")

    implicitWidth: label.implicitWidth + 2 * across + lead
    implicitHeight: label.implicitHeight + 2 * down
    width: Math.min(implicitWidth, maxWidth)
    height: label.height + 2 * down

    Rectangle {
        visible: Style.chip === "pill"
        anchors.fill: parent
        // a wrapped chip stays a rounded box, not a tall capsule
        radius: Math.min(height / 2, Kirigami.Units.gridUnit)
        color: "transparent"
        border.width: 1
        border.color: Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.7)
    }

    Rectangle {
        visible: Style.chip === "edge"
        anchors.fill: parent
        radius: Style.shapeSmall
        color: Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.04)
        border.width: 1
        border.color: Style.inner(Kirigami.Theme)
    }

    Rectangle {
        visible: Style.chip === "edge"
        width: 3
        height: parent.height
        color: Kirigami.Theme.highlightColor
    }

    Rectangle {
        visible: Style.chip === "hairline"
        anchors.fill: parent
        radius: Style.shapeSmall
        color: "transparent"
        border.width: 1
        // dim gold, as the accent is
        border.color: Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.4)
    }

    Rectangle {
        visible: Style.chip === "box"
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.6)
    }

    Plate {
        visible: Style.chip === "plate" || Style.chip === "outline"
        anchors.fill: parent
        fill: Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.06)
        line: Style.inner(Kirigami.Theme)
        notch: Style.plain ? 0 : Kirigami.Units.smallSpacing
        radius: Style.shapeSmall
    }

    Rectangle {
        visible: chip.tick
        x: 0
        y: (parent.height - height) / 2
        width: Kirigami.Units.smallSpacing * 1.5
        height: width
        color: Kirigami.Theme.highlightColor
    }

    Rectangle {
        visible: chip.tick
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Style.inner(Kirigami.Theme)
    }

    QQC2.Label {
        id: label

        x: chip.across + chip.lead
        y: chip.down
        width: Math.max(0, chip.width - 2 * chip.across - chip.lead)
        // the kind comes first and quieter than the stat
        text: chip.kind === "" ? chip.text : "<font color=\"" + Kirigami.Theme.disabledTextColor + "\">" + chip.kind + "</font> " + chip.escaped
        textFormat: chip.kind === "" ? Text.PlainText : Text.StyledText
        wrapMode: Text.Wrap
        font.capitalization: Style.chip === "box" ? Font.AllUppercase : Font.MixedCase
        font.letterSpacing: Style.chip === "box" ? 0.8 : 0
    }
}
