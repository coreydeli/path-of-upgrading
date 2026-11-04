// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Text in the colour of what it means for you.
QQC2.Label {
    property int tone: Tone.Plain
    // blue for better and orange for worse, in place of green and red
    property bool colorblind: false

    color: Style.tone(tone, colorblind, Kirigami.Theme)
}
