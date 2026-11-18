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

    // Which design is on, which the app sets from the settings. The controls
    // draw both: the app's own, Dossier, and Material Design 3.
    property bool material: false
    // whether the colors on the app are dark
    property bool dark: true
    // M3 baseline secondary container, for the row you're on
    readonly property color secondaryContainer: dark ? "#4A4458" : "#E8DEF8"
    readonly property color onSecondaryContainer: dark ? "#E8DEF8" : "#1D192B"

    // whether the controls move, which Plain decorations turn off
    property bool animated: true
    // how long a change takes in Material, and its standard easing
    readonly property int shortDuration: animated ? 200 : 0
    readonly property int mediumDuration: animated ? 300 : 0
    readonly property var standardEasing: ({
        type: Easing.BezierCurve,
        bezierCurve: [0.2, 0, 0, 1, 1, 1]
    })

    // Material is drawn in density independent pixels, which here are a
    // grid unit in 18ths, so they grow with the text
    readonly property real dp: Kirigami.Units.gridUnit / 18

    // Material's shapes: corners of 4, 8, 12, 16 and 28 dp, and a pill
    readonly property real shapeExtraSmall: 4 * dp
    readonly property real shapeSmall: 8 * dp
    readonly property real shapeMedium: 12 * dp
    readonly property real shapeLarge: 16 * dp
    readonly property real shapeExtraLarge: 28 * dp

    // the family the page titles are in, which the app sets from the settings
    property string headingFamily: "Big Shoulders Display"

    // a color with part of another laid over it
    function tint(base: color, over: color, part: real): color {
        return Kirigami.ColorUtils.tintWithAlpha(base, over, part);
    }

    // small capitals for labels, column heads and buttons
    function labelFont(base: font, scale = 1): font {
        if (material) {
            // label large: medium weight, a little spaced
            return Qt.font({
                pointSize: base.pointSize * scale,
                weight: Font.Medium,
                letterSpacing: 0.1
            });
        }
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
        if (material) {
            return Qt.font({
                pointSize: base.pointSize * scale * 1.5,
                weight: Font.Medium
            });
        }
        return Qt.font({
            family: "Big Shoulders Display",
            pointSize: base.pointSize * scale * 1.5,
            weight: Font.Black
        });
    }

    // titles, in capitals: the page's, the popup's item and the like
    function titleFont(base: font, scale = 1): font {
        if (material) {
            // headline small, in sentence case
            return Qt.font({
                family: headingFamily,
                pointSize: base.pointSize * scale * 1.5,
                weight: Font.Normal
            });
        }
        return Qt.font({
            family: headingFamily,
            pointSize: base.pointSize * scale * 1.45,
            weight: Font.ExtraBold,
            capitalization: Font.AllUppercase,
            letterSpacing: 0.5
        });
    }
}
