// SPDX-License-Identifier: MIT
pragma Singleton

import QtQuick
import org.kde.kirigami as Kirigami

// What the controls and the app's own parts share and a color scheme can't
// say: the paper and ink, and the faces the labels and the figures are set in.
QtObject {
    // paper for the slabs and the fields, with ink on it
    readonly property color paper: "#ece6da"
    readonly property color ink: "#0e0e0f"

    // the family the page titles are in, which the app sets from the settings
    property string headingFamily: "Big Shoulders Display"

    // a color with part of another laid over it
    function tint(base: color, over: color, part: real): color {
        return Kirigami.ColorUtils.tintWithAlpha(base, over, part);
    }

    // small capitals for labels, column heads and buttons
    function labelFont(base: font, scale = 1): font {
        return Qt.font({
            family: "Big Shoulders Text",
            pointSize: base.pointSize * scale * 1.02,
            weight: Font.Bold,
            capitalization: Font.AllUppercase,
            letterSpacing: 1.4
        });
    }

    // big numbers: the score, a change
    function figureFont(base: font, scale = 1): font {
        return Qt.font({
            family: "Big Shoulders Display",
            pointSize: base.pointSize * scale * 1.5,
            weight: Font.Black
        });
    }

    // titles, in capitals: the page's, the popup's item and the like
    function titleFont(base: font, scale = 1): font {
        return Qt.font({
            family: headingFamily,
            pointSize: base.pointSize * scale * 1.45,
            weight: Font.ExtraBold,
            capitalization: Font.AllUppercase,
            letterSpacing: 0.5
        });
    }
}
