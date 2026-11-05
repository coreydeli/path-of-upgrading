// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kirigami as Kirigami

// A rule that fades out at both ends, with a small diamond in the middle.
Item {
    id: ornament

    // the diamond is left out for a plain rule
    property bool diamond: true

    implicitHeight: diamond ? 7 : 1
    implicitWidth: Kirigami.Units.gridUnit * 6
    // it's decoration, not something to read out
    Accessible.ignored: true

    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0
                color: Style.clear(Style.frame(Kirigami.Theme))
            }
            GradientStop {
                position: 0.5
                color: Style.frame(Kirigami.Theme)
            }
            GradientStop {
                position: 1
                color: Style.clear(Style.frame(Kirigami.Theme))
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        visible: ornament.diamond
        width: 5
        height: 5
        rotation: 45
        color: Style.gold(Kirigami.Theme)
    }
}
