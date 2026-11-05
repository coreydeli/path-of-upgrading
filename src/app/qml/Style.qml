// SPDX-License-Identifier: MIT
pragma Singleton

import QtQuick
import org.kde.kirigami as Kirigami

// What a color scheme can't say: the game's item colors, the grades, the
// frames and the heading font. The functions that follow the desktop's colors
// when the app's own are off take the Kirigami.Theme of what they color.
QtObject {
    // set by the main window, from the Appearance settings
    property bool appColors: false
    // the family the headings are set in, which is Cinzel unless it's changed
    property string headingFamily: "Cinzel"
    // Whether the extras are left out: the corner pieces, the gauges' rings
    // and the popup's fade.
    property bool plain: false

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
        if (appColors) {
            switch (grade) {
            case "S":
                return "#d4b06a";
            case "A":
                return "#7fb069";
            case "B":
                return "#8fb3c9";
            case "C":
                return "#c9b48f";
            case "D":
                return "#948776";
            }
            return theme.textColor;
        }
        switch (grade) {
        case "S":
            return theme.highlightColor;
        case "A":
            return theme.positiveTextColor;
        case "C":
            return theme.neutralTextColor;
        case "D":
            return theme.disabledTextColor;
        }
        return theme.textColor;
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

    // the line round a frame, and the finer one inside it
    function frame(theme): color {
        if (appColors)
            return "#5c4a32";
        return Kirigami.ColorUtils.linearInterpolation(theme.backgroundColor, theme.textColor, 0.2);
    }

    function inner(theme): color {
        if (appColors)
            return "#2b241c";
        return Kirigami.ColorUtils.linearInterpolation(theme.backgroundColor, theme.textColor, 0.1);
    }

    // what the frames' diamonds and the lit edges are in
    function gold(theme): color {
        return appColors ? "#d4b06a" : theme.highlightColor;
    }

    // a color with part of another laid over it
    function tint(base: color, over: color, part: real): color {
        return Kirigami.ColorUtils.tintWithAlpha(base, over, part);
    }

    // the same color, but see through
    function clear(from: color): color {
        return Qt.rgba(from.r, from.g, from.b, 0);
    }

    // The heading font, the size of the one it's given times scale. Cinzel
    // only has capitals, its lower case are small ones, so it's used as it is.
    function titleFont(base: font, scale = 1): font {
        return Qt.font({
            family: headingFamily,
            pointSize: base.pointSize * scale,
            weight: Font.Bold,
            letterSpacing: 0.5
        });
    }
}
