// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The verdict across the popup, with how good the item is at its right. Each
// look draws it its own way. Dossier and Material have a tinted band with an
// edge, Slate plain words over a 2 px bar, Pocket a rounded tile with the
// verdict in the accent, Edge a thick edge and a short label, Gilt a centered
// line between hairlines with a diamond each side, and Cinder an uppercase
// tag in a colored outline.
Item {
    id: banner

    property string verdict
    property int tone: Tone.Plain
    // blue and orange in place of green and red
    property bool colorblind: false
    // the change, and what it is and what it's against
    property string score
    property string scoreLabel
    property string versus
    // the item's gear score and grade, empty when it has none
    property string gearScore
    property string gearGrade
    // for a row in a list, with smaller type and the gauge small
    property bool compact: false

    readonly property color tint: Style.tone(tone, colorblind, Kirigami.Theme)
    readonly property bool gauged: gearScore !== ""
    readonly property string kind: Style.tile
    readonly property bool edged: kind === "diamonds" || kind === "edge"
    readonly property real edgeWidth: kind === "edge" ? 6 : 3
    readonly property string details: [scoreLabel, versus].filter(part => part !== "").join(", ")

    implicitHeight: content.implicitHeight + 2 * Kirigami.Units.smallSpacing

    Rectangle {
        anchors.fill: parent
        visible: banner.kind === "diamonds" || banner.kind === "edge" || banner.kind === "ring"
        radius: banner.kind === "ring" ? Style.shapeLarge : 0
        color: Style.tint(Kirigami.Theme.backgroundColor, banner.tint, banner.kind === "edge" ? 0.07 : 0.12)
    }

    Rectangle {
        visible: banner.edged
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: parent.left
        }
        width: banner.edgeWidth
        color: banner.tint
    }

    // under the words, in the color of the verdict
    Rectangle {
        visible: banner.kind === "strip"
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
        height: 2
        color: banner.tint
    }

    Repeater {
        model: banner.kind === "hairline" ? 2 : 0

        Rectangle {
            required property int index

            anchors {
                top: index === 0 ? banner.top : undefined
                bottom: index === 1 ? banner.bottom : undefined
                left: banner.left
                right: banner.right
            }
            height: 1
            color: Style.tint(Kirigami.Theme.backgroundColor, Style.accent(Kirigami.Theme), 0.5)
        }
    }

    RowLayout {
        id: content

        anchors {
            fill: parent
            leftMargin: (banner.edged ? banner.edgeWidth : 0) + Kirigami.Units.largeSpacing
            rightMargin: Kirigami.Units.largeSpacing
            topMargin: Kirigami.Units.smallSpacing
            bottomMargin: Kirigami.Units.smallSpacing
        }
        spacing: Kirigami.Units.largeSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                Item {
                    id: tag

                    readonly property real pad: banner.kind === "segments" ? Kirigami.Units.smallSpacing : 0

                    Layout.fillWidth: banner.kind === "hairline"
                    implicitWidth: verdictRow.implicitWidth + 2 * pad
                    implicitHeight: verdictRow.implicitHeight + 2 * pad

                    Rectangle {
                        visible: banner.kind === "segments"
                        anchors.fill: parent
                        color: "transparent"
                        border.width: 2
                        border.color: banner.tint
                    }

                    RowLayout {
                        id: verdictRow

                        anchors.centerIn: parent
                        spacing: Kirigami.Units.largeSpacing

                        Rectangle {
                            visible: banner.kind === "hairline"
                            implicitWidth: 6
                            implicitHeight: 6
                            rotation: 45
                            color: banner.tint
                        }

                        ToneLabel {
                            color: banner.kind === "ring" ? Style.accent(Kirigami.Theme) : banner.tint
                            font: {
                                const base = Kirigami.Theme.defaultFont;
                                if (banner.kind === "edge")
                                    return Style.labelFont(base, banner.compact ? 1.15 : 1.5);
                                if (banner.kind === "segments")
                                    return Style.labelFont(base, banner.compact ? 1 : 1.3);
                                return Style.titleFont(base, banner.compact ? 1.1 : 1.5);
                            }
                            text: banner.verdict
                            tone: banner.tone
                            colorblind: banner.colorblind
                        }

                        Rectangle {
                            visible: banner.kind === "hairline"
                            implicitWidth: 6
                            implicitHeight: 6
                            rotation: 45
                            color: banner.tint
                        }
                    }
                }

                ToneLabel {
                    Layout.fillWidth: banner.kind !== "hairline"
                    horizontalAlignment: Text.AlignRight
                    font: Style.figureFont(Kirigami.Theme.defaultFont, banner.compact ? 1.3 : 2.2)
                    text: banner.score
                    tone: banner.tone
                    colorblind: banner.colorblind
                }
            }

            // what the score is and what it's against: "for your build, vs Grim Crown"
            QQC2.Label {
                Layout.fillWidth: true
                visible: text !== ""
                wrapMode: Text.Wrap
                horizontalAlignment: banner.kind === "hairline" ? Text.AlignHCenter : Text.AlignLeft
                color: Kirigami.Theme.disabledTextColor
                text: banner.details
            }
        }

        ScoreGauge {
            Layout.alignment: Qt.AlignVCenter
            visible: banner.gauged
            score: banner.gearScore
            grade: banner.gearGrade
            size: banner.compact ? "small" : "medium"
        }
    }
}
