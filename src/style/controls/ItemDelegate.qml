// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import PouStyle

// A row you pick, as the sidebar's are. Where you are turns into a paper slab.
T.ItemDelegate {
    id: control

    readonly property color ink: Kirigami.Theme.textColor
    readonly property color ground: Kirigami.Theme.backgroundColor
    readonly property bool here: highlighted || checked

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Kirigami.Units.smallSpacing + 2
    horizontalPadding: Kirigami.Units.largeSpacing
    spacing: Kirigami.Units.smallSpacing
    hoverEnabled: true
    font: Look.labelFont(Kirigami.Theme.defaultFont, 1.1)
    // no icons, as they'd be the desktop's
    icon.name: ""
    icon.source: ""

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.here ? Look.ink : control.ink
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        visible: control.here || control.hovered
        color: control.here ? Look.paper : Look.tint(control.ground, control.ink, 0.07)
    }
}
