// SPDX-License-Identifier: MIT
pragma Singleton

import QtQuick
import org.kde.kirigami as Kirigami
import PouStyle

// What a color scheme can't say: the game's item colors, the grades, the
// rules and the faces the text is set in. The functions that follow the
// desktop's colors when the app's own are off take the Kirigami.Theme of what
// they color.
QtObject {
    id: style

    // set by the main window, from the Appearance settings
    property bool appColors: false
    // the family the page titles are set in
    property string headingFamily: "Big Shoulders Display"
    // Whether the plates are square, which is when the decorations are plain.
    property bool plain: false

    // how much is cut off the corners of a plate
    readonly property real cut: plain ? 0 : 12

    // the paper the slabs and the fields are, and the ink on it
    readonly property color paper: Look.paper
    readonly property color ink: Look.ink

    onHeadingFamilyChanged: Look.headingFamily = headingFamily

    // the item colors the game's players read at a glance
    function rarity(rarity: string, theme): color {
        switch (rarity) {
        case "Normal":
            return "#c8c8c8";
        case "Magic":
            return "#8888ff";
        case "Rare":
            return "#ffff77";
        case "Unique":
            return "#af6025";
        case "Currency":
            return "#aa9e82";
        case "Gem":
            return "#1ba29b";
        default:
            return theme.textColor;
        }
    }

    // gear score grades, best first
    function grade(grade: string, theme): color {
        switch (grade) {
        case "S":
            return accent(theme);
        case "A":
            return theme.positiveTextColor;
        case "C":
            return theme.neutralTextColor;
        case "D":
            return theme.disabledTextColor;
        }
        return theme.textColor;
    }

    // how far up the grades it is, out of five, for the pips
    function gradeStep(grade: string): int {
        return grade ? "DCBAS".indexOf(grade) + 1 : 0;
    }

    // better, worse and better with a catch. Blue and orange for those who
    // can't tell green and red apart.
    function tone(tone: int, colorblind: bool, theme): color {
        switch (tone) {
        case Tone.Good:
            return colorblind ? "#3daee9" : theme.positiveTextColor;
        case Tone.Bad:
            return colorblind ? "#f67400" : theme.negativeTextColor;
        case Tone.Warning:
            return colorblind ? "#fdbc4b" : theme.neutralTextColor;
        default:
            return theme.textColor;
        }
    }

    // the line round a plate, and the finer one inside it
    function frame(theme): color {
        return Kirigami.ColorUtils.linearInterpolation(theme.backgroundColor, theme.textColor, 0.28);
    }

    function inner(theme): color {
        return Kirigami.ColorUtils.linearInterpolation(theme.backgroundColor, theme.textColor, 0.14);
    }

    // the one color besides the text, which is teal in the app's scheme
    function accent(theme): color {
        return theme.highlightColor;
    }

    // a color with part of another laid over it
    function tint(base: color, over: color, part: real): color {
        return Look.tint(base, over, part);
    }

    // the same color, but see through
    function clear(from: color): color {
        return Qt.rgba(from.r, from.g, from.b, 0);
    }

    // titles, in capitals: the page's, the popup's item and the like
    function titleFont(base: font, scale = 1): font {
        return Look.titleFont(base, scale);
    }

    // small capitals for labels, column heads and buttons
    function labelFont(base: font, scale = 1): font {
        return Look.labelFont(base, scale);
    }

    // what sits over a card
    function sectionFont(base: font): font {
        return Look.labelFont(base, 1.15);
    }

    // big numbers: the score, a change
    function figureFont(base: font, scale = 1): font {
        return Look.figureFont(base, scale);
    }
}
