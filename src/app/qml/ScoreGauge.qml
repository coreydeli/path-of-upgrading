// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// A gear score as a ring that fills up to it, in the color of its grade, with
// the grade's letter in the middle. Plain draws "42 C" as text.
Item {
    id: gauge

    // as the report has them, "42" and "C"
    property string score
    property string grade
    // small (in a list), medium (the popup) or large (a page's headline)
    property string size: "medium"

    // out of a hundred, the most the ring shows
    readonly property real fraction: Math.max(0, Math.min(1, Number(score) / 100))
    readonly property color tint: Style.grade(grade, Kirigami.Theme)
    readonly property real diameter: Kirigami.Units.gridUnit * (size === "small" ? 2.2 : size === "large" ? 5 : 3.4)
    readonly property real thickness: size === "small" ? 3 : 4
    // the number goes under the letter when there's room
    readonly property bool numbered: size !== "small"
    readonly property bool ring: !Style.plain
    readonly property font base: Kirigami.Theme.defaultFont

    implicitWidth: ring ? diameter : label.implicitWidth
    implicitHeight: ring ? diameter : label.implicitHeight

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Gear score %1, grade %2").arg(score).arg(grade)

    Shape {
        anchors.fill: parent
        visible: gauge.ring
        preferredRendererType: Shape.CurveRenderer
        // it's round, so the edges are worth smoothing
        layer.enabled: true
        layer.samples: 4

        ShapePath {
            fillColor: "transparent"
            strokeColor: Style.frame(Kirigami.Theme)
            strokeWidth: gauge.thickness
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: gauge.width / 2
                centerY: gauge.height / 2
                radiusX: (gauge.width - gauge.thickness) / 2
                radiusY: (gauge.height - gauge.thickness) / 2
                startAngle: 135
                sweepAngle: 270
            }
        }

        ShapePath {
            fillColor: "transparent"
            strokeColor: gauge.tint
            strokeWidth: gauge.thickness
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: gauge.width / 2
                centerY: gauge.height / 2
                radiusX: (gauge.width - gauge.thickness) / 2
                radiusY: (gauge.height - gauge.thickness) / 2
                startAngle: 135
                sweepAngle: 270 * gauge.fraction
            }
        }
    }

    Column {
        anchors.centerIn: parent
        visible: gauge.ring
        spacing: 0

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: gauge.tint
            font: Style.titleFont(gauge.base,
                                  gauge.size === "large" ? 2.2 : gauge.size === "medium" ? 1.5 : 1.1)
            text: gauge.grade
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: gauge.numbered
            color: Kirigami.Theme.textColor
            font: Kirigami.Theme.smallFont
            text: gauge.score
        }
    }

    Text {
        id: label

        anchors.centerIn: parent
        visible: !gauge.ring
        color: gauge.tint
        font.bold: true
        text: gauge.score + " " + gauge.grade
    }
}
