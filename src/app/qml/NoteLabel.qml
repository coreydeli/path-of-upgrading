// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The label of a gear note, "Weak rolls", which says what it means when you
// hover it.
QQC2.Label {
    id: label

    // what it means, empty for nothing to say
    property string help

    Layout.alignment: Qt.AlignTop
    Layout.preferredWidth: Kirigami.Units.gridUnit * 5
    // level with the text in the first chip
    topPadding: Kirigami.Units.smallSpacing
    color: Kirigami.Theme.disabledTextColor
    Accessible.description: help

    HoverHandler {
        id: hover

        enabled: label.help !== ""
        cursorShape: Qt.WhatsThisCursor
    }

    QQC2.ToolTip.text: help
    QQC2.ToolTip.visible: hover.hovered
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
}
