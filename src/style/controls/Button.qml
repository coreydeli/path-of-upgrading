// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import PouStyle

// No icons: they'd come from the desktop's icon theme, which Windows doesn't
// have and every Linux desktop draws differently.
T.Button {
    id: root

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    verticalPadding: Look.material ? Kirigami.Units.smallSpacing : Kirigami.Units.smallSpacing + 1
    horizontalPadding: Look.material ? 24 * Look.dp : Kirigami.Units.largeSpacing + 2
    hoverEnabled: true

    contentItem: ButtonText {
        control: root
    }

    background: ButtonFace {
        control: root
    }
}
