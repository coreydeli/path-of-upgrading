// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// What a page sits on: the window's color, with a soft warm light at the top
// in the middle when the app's colors are on. Plain leaves the light out.
Rectangle {
    id: glow

    // whether there's a light at all
    readonly property bool lit: Style.appColors && !Style.plain
    readonly property color warm: "#1d1813"

    color: Kirigami.Theme.backgroundColor

    Shape {
        anchors.fill: parent
        visible: glow.lit
        preferredRendererType: Shape.GeometryRenderer

        ShapePath {
            strokeColor: "transparent"
            startX: 0
            startY: 0

            fillGradient: RadialGradient {
                centerX: glow.width / 2
                centerY: 0
                centerRadius: Math.max(glow.width * 0.55, Kirigami.Units.gridUnit * 20)
                focalX: centerX
                focalY: centerY

                GradientStop {
                    position: 0
                    color: glow.warm
                }
                GradientStop {
                    position: 1
                    color: Style.clear(glow.warm)
                }
            }

            PathLine {
                x: glow.width
                y: 0
            }
            PathLine {
                x: glow.width
                y: glow.height
            }
            PathLine {
                x: 0
                y: glow.height
            }
        }
    }
}
