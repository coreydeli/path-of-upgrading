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
    // Which look is on, by its id: Dossier, the app's own, Material Design 3
    // and the rest. Each has its entry in Look's table, and the controls draw
    // them all from the same colors.
    property string design: "dossier"
    readonly property bool dossier: Look.dossier
    readonly property bool material: Look.material
    // Whether it's drawn on the window's own background in soft shapes, as
    // every look but Dossier is, and whether it has round pills and its cards
    // framed.
    readonly property bool flat: Look.flat
    readonly property bool pill: Look.pill
    readonly property string chip: Look.chip
    readonly property bool framed: Look.framed
    // whether the colors on the app are dark, which Material's depend on
    property bool dark: true
    // Whether the plates are square, which is when the decorations are plain.
    property bool plain: false

    // how much is cut off the corners of a plate
    readonly property real cut: plain || !dossier ? 0 : 12

    // the paper the slabs and the fields are, and the ink on it
    readonly property color paper: Look.paper
    readonly property color ink: Look.ink

    onHeadingFamilyChanged: Look.headingFamily = headingFamily
    onDesignChanged: Look.design = design
    onPlainChanged: Look.animated = !plain
    onDarkChanged: Look.dark = dark

    // The shape scale and the motion are in Look, as the controls draw them
    readonly property real shapeSmall: Look.shapeSmall
    readonly property real shapeMedium: Look.shapeMedium
    readonly property real shapeLarge: Look.shapeLarge
    readonly property real shapeExtraLarge: Look.shapeExtraLarge
    readonly property int shortDuration: Look.shortDuration
    readonly property int mediumDuration: Look.mediumDuration

    // The item colors the game's players read at a glance. On a light surface
    // they're darkened, each to 4.5 to 1 or better against Material's.
    function rarity(rarity: string, theme): color {
        const light = !dossier && theme.backgroundColor.hslLightness > 0.6;
        switch (rarity) {
        case "Normal":
            return light ? "#4a4a4a" : "#c8c8c8";
        case "Magic":
            return light ? "#3b3bcc" : "#8888ff";
        case "Rare":
            return light ? "#6f6000" : "#ffff77";
        case "Unique":
            return light ? "#8a4a12" : "#af6025";
        case "Currency":
            return light ? "#6b5f3f" : "#aa9e82";
        case "Gem":
            return light ? "#00706b" : "#1ba29b";
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
        return Kirigami.ColorUtils.linearInterpolation(theme.backgroundColor, theme.textColor, Look.spec.line);
    }

    function inner(theme): color {
        return Kirigami.ColorUtils.linearInterpolation(theme.backgroundColor, theme.textColor, Look.spec.rule);
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
