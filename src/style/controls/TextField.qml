// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import PouStyle

T.TextField {
    id: root

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)
    padding: Kirigami.Units.smallSpacing
    leftPadding: Look.material ? 16 * Look.dp : Kirigami.Units.smallSpacing * 2
    hoverEnabled: true

    color: Look.material ? Kirigami.Theme.textColor : Look.ink
    selectionColor: Kirigami.Theme.highlightColor
    selectedTextColor: Look.material ? Kirigami.Theme.highlightedTextColor : Look.paper
    placeholderTextColor: Look.material ? Look.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.6)
                                        : Look.tint(Look.paper, Look.ink, 0.45)
    verticalAlignment: TextInput.AlignVCenter

    Text {
        id: placeholder

        x: root.leftPadding
        y: root.topPadding
        width: root.width - (root.leftPadding + root.rightPadding)
        height: root.height - (root.topPadding + root.bottomPadding)
        text: root.placeholderText
        font: root.font
        color: root.placeholderTextColor
        verticalAlignment: root.verticalAlignment
        visible: !root.length && !root.preeditText && (!root.activeFocus || root.horizontalAlignment !== Qt.AlignHCenter)
        elide: Text.ElideRight
    }

    background: Field {
        control: root
    }
}
