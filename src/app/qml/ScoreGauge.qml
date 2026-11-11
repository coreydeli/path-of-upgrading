// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

// A gear score: the number in big condensed type with five diamonds under it
// for the grade, lit up to its step, and the grade's letter beside them.
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

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Gear score %1, grade %2").arg(score).arg(grade)

    Column {
        id: column

        spacing: 2

        Text {
            anchors.right: parent.right
            font: Style.figureFont(gauge.base, gauge.size === "small" ? 1 : gauge.size === "large" ? 2.8 : 2)
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

                Rectangle {
                    required property int index

                    width: gauge.side
                    height: gauge.side
                    y: gauge.side * 0.2
                    rotation: 45
                    antialiasing: true
                    color: index < gauge.lit ? gauge.tint : "transparent"
                    border.width: 1
                    border.color: index < gauge.lit ? gauge.tint : Style.tint(Kirigami.Theme.backgroundColor,
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
}
