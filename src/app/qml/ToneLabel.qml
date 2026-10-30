// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Text in the colour of what it means for you.
QQC2.Label {
    property int tone: Tone.Plain
    // blue for better and orange for worse, in place of green and red
    property bool colorblind: false

    color: {
        switch (tone) {
        case Tone.Good:
            return colorblind ? "#3daee9" : Kirigami.Theme.positiveTextColor;
        case Tone.Bad:
            return colorblind ? "#f67400" : Kirigami.Theme.negativeTextColor;
        case Tone.Warning:
            return colorblind ? "#fdbc4b" : Kirigami.Theme.neutralTextColor;
        default:
            return Kirigami.Theme.textColor;
        }
    }
}
