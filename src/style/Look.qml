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

    // Which look is on, by its id, which the app sets from the settings. The
    // controls and the parts draw it from its entry in `looks`.
    property string design: "dossier"
    readonly property bool dossier: design === "dossier"
    readonly property bool material: design === "material"
    // Whether the look is drawn with soft shapes on the window's own
    // background, as every look but Dossier is, not on slabs of paper.
    readonly property bool flat: !dossier
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

    // The looks, each one a line of what the parts draw differently. Corners
    // are in dp: extra small is for fields, small for chips, medium for cards,
    // large for the popup and extra large for the sidebar's picked row, and
    // `button` is a button's, unless the look is a `pill`, which makes buttons,
    // chips and scroll bars fully round. `line` and `rule` say how much of the
    // text's color the frame and the finer rules have, `chip` is how a stat
    // chip is drawn and `tile` how the gear score and the verdict are. The fonts are for labels (small, in `caps` or not),
    // figures (the score) and titles, whose family is the heading font's; a
    // missing family is the text's.
    readonly property var looks: ({
        "dossier": {
            tile: "diamonds",
            xs: 0, s: 0, m: 0, l: 0, xl: 0, button: 0, pill: false,
            line: 0.28, rule: 0.14, framed: true, chip: "plate",
            label: { family: "Big Shoulders Text", weight: Font.Bold, caps: true, spacing: 1.4, scale: 1.02 },
            figure: { family: "Big Shoulders Display", weight: Font.Black, scale: 1.5 },
            title: { weight: Font.ExtraBold, caps: true, spacing: 0.5, scale: 1.45 }
        },
        "material": {
            tile: "diamonds",
            xs: 4, s: 8, m: 12, l: 16, xl: 28, button: 20, pill: true,
            line: 0.2, rule: 0.12, framed: false, chip: "outline",
            label: { weight: Font.Medium, caps: false, spacing: 0.1, scale: 1 },
            figure: { weight: Font.Medium, scale: 1.5 },
            title: { weight: Font.Normal, caps: false, spacing: 0, scale: 1.5 }
        },
        "slate": {
            tile: "strip",
            xs: 2, s: 2, m: 2, l: 2, xl: 2, button: 2, pill: false,
            line: 0.18, rule: 0.12, framed: true, chip: "tick",
            label: { weight: Font.Medium, caps: false, spacing: 0.2, scale: 1 },
            figure: { weight: Font.Bold, scale: 1.5 },
            title: { weight: Font.Bold, caps: false, spacing: 0.3, scale: 1.45 }
        },
        "pocket": {
            tile: "ring",
            xs: 8, s: 14, m: 14, l: 14, xl: 14, button: 14, pill: true,
            line: 0.14, rule: 0.1, framed: true, chip: "pill",
            label: { weight: Font.DemiBold, caps: false, spacing: 0, scale: 1 },
            figure: { weight: Font.Bold, scale: 1.7 },
            title: { weight: Font.Bold, caps: false, spacing: 0, scale: 1.5 }
        },
        "edge": {
            tile: "edge",
            xs: 3, s: 3, m: 3, l: 3, xl: 3, button: 3, pill: false,
            line: 0.2, rule: 0.12, framed: true, chip: "edge",
            label: { family: "Barlow Semi Condensed", weight: Font.Medium, caps: true, spacing: 0.6, scale: 1 },
            figure: { family: "Barlow Semi Condensed", weight: Font.Bold, scale: 1.6 },
            title: { weight: Font.Bold, caps: false, spacing: 0.2, scale: 1.5 }
        },
        "gilt": {
            tile: "hairline",
            xs: 2, s: 2, m: 4, l: 4, xl: 4, button: 6, pill: false,
            line: 0.3, rule: 0.16, framed: true, chip: "hairline",
            label: { family: "Cinzel", weight: Font.Medium, caps: false, spacing: 1.2, scale: 0.92 },
            figure: { family: "Cinzel", weight: Font.DemiBold, scale: 1.7 },
            title: { weight: Font.DemiBold, caps: false, spacing: 1, scale: 1.4 }
        },
        "cinder": {
            tile: "segments",
            xs: 0, s: 0, m: 0, l: 0, xl: 0, button: 0, pill: false,
            line: 0.3, rule: 0.16, framed: true, chip: "box", shadow: 4,
            label: { family: "JetBrains Mono", weight: Font.Medium, caps: true, spacing: 0.9, scale: 0.85 },
            figure: { family: "JetBrains Mono", weight: Font.Bold, scale: 1.6 },
            title: { weight: Font.Bold, caps: true, spacing: 0.8, scale: 1.35 },
            brand: { family: "Silkscreen", weight: Font.Normal, caps: false, spacing: 0.5, scale: 1 }
        }
    })
    readonly property var spec: looks[design] ?? looks["dossier"]

    // the shape scale, as the controls draw it
    readonly property real shapeExtraSmall: spec.xs * dp
    readonly property real shapeSmall: spec.s * dp
    readonly property real shapeMedium: spec.m * dp
    readonly property real shapeLarge: spec.l * dp
    readonly property real shapeExtraLarge: spec.xl * dp
    readonly property real shapeButton: spec.button * dp
    readonly property bool pill: spec.pill
    // whether the cards have a line round them
    readonly property bool framed: spec.framed
    // how a stat chip is drawn: plate, outline, tick, pill, edge, hairline
    // or box, and `shadow` is the width in pixels of a hard shadow under a card, if it has one
    readonly property string chip: spec.chip
    readonly property real shadow: spec.shadow ?? 0
    // how the score and the verdict are drawn: diamonds, strip, ring, edge,
    // hairline or segments
    readonly property string tile: spec.tile

    // the family the page titles are in, which the app sets from the settings
    property string headingFamily: "Big Shoulders Display"

    // a color with part of another laid over it
    function tint(base: color, over: color, part: real): color {
        return Kirigami.ColorUtils.tintWithAlpha(base, over, part);
    }

    // a font from an entry of the look's: its family when it has one
    function faceOf(face: var, base: font, scale: real, extra: var): font {
        const made = {
            pointSize: base.pointSize * scale * face.scale,
            weight: face.weight
        };
        if (face.family)
            made.family = face.family;
        for (const key in extra)
            made[key] = extra[key];
        return Qt.font(made);
    }

    // small capitals for labels, column heads and buttons
    function labelFont(base: font, scale = 1): font {
        const face = spec.label;
        return faceOf(face, base, scale, {
            capitalization: face.caps ? Font.AllUppercase : Font.MixedCase,
            letterSpacing: face.spacing
        });
    }

    // big numbers: the score, a change
    function figureFont(base: font, scale = 1): font {
        return faceOf(spec.figure, base, scale, {});
    }

    // the app's name: in the look's own face when it has one, else a title
    function brandFont(base: font, scale = 1): font {
        const face = spec.brand;
        if (!face)
            return titleFont(base, scale);
        return faceOf(face, base, scale, {
            capitalization: face.caps ? Font.AllUppercase : Font.MixedCase,
            letterSpacing: face.spacing
        });
    }

    // titles, in capitals in some looks: the page's, the popup's item and the like
    function titleFont(base: font, scale = 1): font {
        const face = spec.title;
        return faceOf(face, base, scale, {
            family: headingFamily,
            capitalization: face.caps ? Font.AllUppercase : Font.MixedCase,
            letterSpacing: face.spacing
        });
    }
}
