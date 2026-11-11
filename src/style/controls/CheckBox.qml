// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami

T.CheckBox {
    id: root

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitIndicatorWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)
    padding: 2
    spacing: Kirigami.Units.smallSpacing * 2
    hoverEnabled: true

    indicator: Mark {
        x: root.text ? root.leftPadding : root.leftPadding + (root.availableWidth - width) / 2
        y: root.topPadding + (root.availableHeight - height) / 2
        control: root
        round: false
        twoCells: false
    }

    contentItem: Text {
        leftPadding: root.text ? root.indicator.width + root.spacing : 0
        text: root.text
        font: root.font
        color: Kirigami.Theme.textColor
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        opacity: root.enabled ? 1 : 0.4
    }
}
