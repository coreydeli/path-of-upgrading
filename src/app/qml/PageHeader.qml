// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The top of a page: its title large, a line saying what the page is for, and
// a rule under them.
ColumnLayout {
    id: header

    property string title
    property string description

    spacing: Kirigami.Units.smallSpacing

    QQC2.Label {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.leftMargin: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
        elide: Text.ElideRight
        font: Style.titleFont(Kirigami.Theme.defaultFont, 1.8)
        text: header.title
        Accessible.role: Accessible.Heading
    }

    QQC2.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
        objectName: "pageDescription"
        visible: header.description !== ""
        color: Kirigami.Theme.disabledTextColor
        text: header.description
        wrapMode: Text.WordWrap
    }

    Ornament {
        Layout.fillWidth: true
        Layout.bottomMargin: Kirigami.Units.smallSpacing
        diamond: !Style.plain
    }
}
