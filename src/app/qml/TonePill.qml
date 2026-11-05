// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// A small rounded tag in the color of what it means for you, for a word like
// "Better" or "Instant buyout".
Rectangle {
    id: pill

    property string text
    property int tone: Tone.Plain
    // blue for better and orange for worse, in place of green and red
    property bool colorblind: false

    readonly property color tint: Style.tone(tone, colorblind, Kirigami.Theme)

    implicitWidth: label.implicitWidth + 2 * Kirigami.Units.smallSpacing + 2
    implicitHeight: label.implicitHeight + 2
    radius: height / 2
    color: Style.tint(Kirigami.Theme.backgroundColor, tint, 0.15)
    border {
        width: 1
        color: Style.tint(Kirigami.Theme.backgroundColor, tint, 0.4)
    }

    QQC2.Label {
        id: label

        anchors.centerIn: parent
        color: pill.tint
        font: Kirigami.Theme.smallFont
        text: pill.text
    }
}
