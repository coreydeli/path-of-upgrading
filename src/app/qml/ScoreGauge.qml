// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// A gear score, drawn as the look draws it. Dossier and Material set the
// number in big type with five diamonds under it for the grade, lit up to its
// step. Slate has the grade beside the number in the accent and a thin bar
// under it, Pocket a ring round the number, Edge a thick edge in the grade's
// color with a thin meter, Gilt the number between two hairlines and Cinder a
// bar of squares filled up to the score.
Item {
    id: gauge

    // as the report has them, "42" and "C"
    property string score
    property string grade
    // small (in a list), medium (the popup) or large (a page's headline)
    property string size: "medium"

    // what it says in a line, "42 C"
    readonly property string text: score + " " + grade
    // out of a hundred
    readonly property real fraction: Math.max(0, Math.min(1, Number(score) / 100))
    readonly property color tint: Style.grade(grade, Kirigami.Theme)
    readonly property int lit: Style.gradeStep(grade)
    readonly property real side: size === "small" ? 4 : size === "large" ? 8 : 6
    readonly property font base: Kirigami.Theme.defaultFont
    // how much bigger than the text the number is
    readonly property real figureScale: size === "small" ? 1 : size === "large" ? 2.8 : 2
    readonly property color track: Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
    readonly property color hairline: Style.tint(Kirigami.Theme.backgroundColor, Style.accent(Kirigami.Theme), 0.5)

    implicitWidth: {
        switch (Style.tile) {
        case "strip":
            return strip.implicitWidth;
        case "ring":
            return ring.implicitWidth;
        case "edge":
            return edge.implicitWidth;
        case "hairline":
            return hairlines.implicitWidth;
        case "segments":
            return segments.implicitWidth;
        }
        return diamonds.implicitWidth;
    }
    implicitHeight: {
        switch (Style.tile) {
        case "strip":
            return strip.implicitHeight;
        case "ring":
            return ring.implicitHeight;
        case "edge":
            return edge.implicitHeight;
        case "hairline":
            return hairlines.implicitHeight;
        case "segments":
            return segments.implicitHeight;
        }
        return diamonds.implicitHeight;
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Gear score %1, grade %2").arg(score).arg(grade)

    Column {
        id: diamonds

        visible: Style.tile === "diamonds"
        spacing: 2

        Text {
            anchors.right: parent.right
            font: Style.figureFont(gauge.base, gauge.figureScale)
            color: Kirigami.Theme.textColor
            text: gauge.score
            // the face's figures sit high, so the diamonds come up under them
            bottomPadding: -Math.round(font.pointSize * 0.12)
        }

        Row {
            anchors.right: parent.right
            spacing: gauge.size === "small" ? 2 : 4

            Repeater {
                model: 5

                // the gauge is gone before its diamonds when a window is torn
                // down, hence the question marks
                Rectangle {
                    required property int index
                    readonly property bool on: index < (gauge?.lit ?? 0)

                    width: gauge?.side ?? 0
                    height: width
                    y: width * 0.2
                    rotation: 45
                    antialiasing: true
                    color: on ? gauge.tint : "transparent"
                    border.width: 1
                    border.color: on ? gauge.tint : Style.tint(Kirigami.Theme.backgroundColor,
                                                               Kirigami.Theme.textColor, 0.35)
                }
            }

            Text {
                visible: gauge.size !== "small"
                font: Style.labelFont(Kirigami.Theme.smallFont)
                color: gauge.tint
                text: gauge.grade
                topPadding: -3
            }
        }
    }

    // the number with its grade beside it, and a 2 px bar under both
    Column {
        id: strip

        visible: Style.tile === "strip"
        spacing: 2

        Row {
            id: stripRow

            anchors.right: parent.right
            spacing: Kirigami.Units.smallSpacing

            Text {
                font: Style.figureFont(gauge.base, gauge.figureScale)
                color: Kirigami.Theme.textColor
                text: gauge.score
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: gauge.size !== "small"
                font: Style.labelFont(gauge.base, 1.3)
                color: Style.accent(Kirigami.Theme)
                text: gauge.grade
            }
        }

        Rectangle {
            width: stripRow.implicitWidth
            height: 2
            color: gauge.track

            Rectangle {
                width: parent.width * gauge.fraction
                height: parent.height
                color: gauge.tint
            }
        }
    }

    // the number in a ring that fills up to the score
    Item {
        id: ring

        readonly property real diameter: Kirigami.Units.gridUnit * (gauge.size === "small" ? 2.2 : gauge.size === "large" ? 5.2 : 3.6)
        readonly property real stroke: gauge.size === "small" ? 3 : 5

        visible: Style.tile === "ring"
        implicitWidth: diameter
        implicitHeight: diameter

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: gauge.track
                strokeWidth: ring.stroke

                PathAngleArc {
                    centerX: ring.diameter / 2
                    centerY: ring.diameter / 2
                    radiusX: (ring.diameter - ring.stroke) / 2
                    radiusY: radiusX
                    startAngle: -90
                    sweepAngle: 359.9
                }
            }

            ShapePath {
                fillColor: "transparent"
                strokeColor: gauge.tint
                strokeWidth: ring.stroke
                capStyle: ShapePath.RoundCap

                PathAngleArc {
                    centerX: ring.diameter / 2
                    centerY: ring.diameter / 2
                    radiusX: (ring.diameter - ring.stroke) / 2
                    radiusY: radiusX
                    startAngle: -90
                    sweepAngle: Math.min(359.9, 360 * gauge.fraction)
                }
            }
        }

        Column {
            anchors.centerIn: parent

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                font: Style.figureFont(gauge.base, gauge.size === "small" ? 0.9 : gauge.size === "large" ? 2 : 1.4)
                color: Kirigami.Theme.textColor
                text: gauge.score
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: gauge.size !== "small"
                font: Style.labelFont(Kirigami.Theme.smallFont)
                color: gauge.tint
                text: gauge.grade
            }
        }
    }

    // a thick edge in the grade's color, the number, and a thin meter
    Item {
        id: edge

        visible: Style.tile === "edge"
        implicitWidth: 4 + Kirigami.Units.smallSpacing + edgeColumn.implicitWidth
        implicitHeight: edgeColumn.implicitHeight

        Rectangle {
            width: gauge.size === "small" ? 3 : 4
            height: parent.height
            color: gauge.tint
        }

        Column {
            id: edgeColumn

            x: 4 + Kirigami.Units.smallSpacing
            spacing: 2

            Row {
                id: edgeRow

                spacing: Kirigami.Units.smallSpacing

                Text {
                    font: Style.figureFont(gauge.base, gauge.figureScale)
                    color: Kirigami.Theme.textColor
                    text: gauge.score
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: gauge.size !== "small"
                    font: Style.labelFont(gauge.base, 1.2)
                    color: gauge.tint
                    text: gauge.grade
                }
            }

            Rectangle {
                width: edgeRow.implicitWidth
                height: 2
                color: gauge.track

                Rectangle {
                    width: parent.width * gauge.fraction
                    height: parent.height
                    color: gauge.tint
                }
            }
        }
    }

    // a serif number between two thin lines, the grade in small caps under it
    Column {
        id: hairlines

        visible: Style.tile === "hairline"
        spacing: 2

        Rectangle {
            width: hairNumber.implicitWidth
            height: 1
            color: gauge.hairline
        }

        Text {
            id: hairNumber

            anchors.horizontalCenter: parent.horizontalCenter
            leftPadding: Kirigami.Units.largeSpacing
            rightPadding: Kirigami.Units.largeSpacing
            font: Style.figureFont(gauge.base, gauge.figureScale)
            color: Kirigami.Theme.textColor
            text: gauge.score
        }

        Rectangle {
            width: hairNumber.implicitWidth
            height: 1
            color: gauge.hairline
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: gauge.size !== "small"
            font: Style.labelFont(Kirigami.Theme.smallFont)
            color: gauge.tint
            text: gauge.grade
        }
    }

    // a monospace number over ten squares, filled up to the score
    Column {
        id: segments

        visible: Style.tile === "segments"
        spacing: 3

        Text {
            anchors.right: parent.right
            font: Style.figureFont(gauge.base, gauge.figureScale)
            color: Kirigami.Theme.textColor
            text: gauge.score
        }

        Row {
            anchors.right: parent.right
            spacing: 2

            Repeater {
                model: 10

                Rectangle {
                    required property int index
                    readonly property bool on: index < Math.round((gauge?.fraction ?? 0) * 10)

                    width: gauge?.side ?? 0
                    height: width
                    color: on ? gauge.tint : "transparent"
                    border.width: 1
                    border.color: on ? gauge.tint : Style.tint(Kirigami.Theme.backgroundColor,
                                                               Kirigami.Theme.textColor, 0.35)
                }
            }
        }
    }
}
