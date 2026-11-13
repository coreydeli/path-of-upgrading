// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Shapes

// A panel with its top left and bottom right corners cut off, which is how
// the app draws its cards and the popup. Plain decorations square it.
Shape {
    id: plate

    property color fill: "transparent"
    property color line: "transparent"
    property real lineWidth: 1
    property real notch: Style.cut

    // the line is drawn on the middle of its width, so it's kept inside
    readonly property real h: lineWidth / 2

    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        fillColor: plate.fill
        strokeColor: plate.line
        strokeWidth: plate.line.a > 0 ? plate.lineWidth : -1
        joinStyle: ShapePath.MiterJoin
        startX: plate.notch + plate.h
        startY: plate.h

        PathLine {
            x: plate.width - plate.h
            y: plate.h
        }

        PathLine {
            x: plate.width - plate.h
            y: plate.height - plate.notch - plate.h
        }

        PathLine {
            x: plate.width - plate.notch - plate.h
            y: plate.height - plate.h
        }

        PathLine {
            x: plate.h
            y: plate.height - plate.h
        }

        PathLine {
            x: plate.h
            y: plate.notch + plate.h
        }

        PathLine {
            x: plate.notch + plate.h
            y: plate.h
        }
    }
}
