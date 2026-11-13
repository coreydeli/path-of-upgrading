// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The top of a page: its title in a slab of paper, and a line saying what the
// page is for.
ColumnLayout {
    id: header

    property string title
    property string description

    readonly property real inset: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing

    spacing: Kirigami.Units.smallSpacing

    // Takes the width it's given and wants none, so how wide the slab is
    // doesn't feed back into how wide the header is.
    Item {
        Layout.topMargin: Kirigami.Units.largeSpacing + 2
        Layout.fillWidth: true
        Layout.preferredWidth: 0
        implicitHeight: slab.implicitHeight

        Rectangle {
            x: header.inset
            width: Math.min(slab.implicitWidth + 2 * Kirigami.Units.largeSpacing,
                            parent.width - 2 * header.inset)
            height: parent.height
            color: Style.paper
            Accessible.role: Accessible.Heading
            Accessible.name: header.title

            Text {
                id: slab

                anchors.centerIn: parent
                width: Math.min(implicitWidth, parent.width - 2 * Kirigami.Units.largeSpacing)
                elide: Text.ElideRight
                font: Style.titleFont(Kirigami.Theme.defaultFont, 1.8)
                color: Style.ink
                text: header.title
            }
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        Layout.leftMargin: header.inset
        Layout.rightMargin: header.inset
        Layout.bottomMargin: Kirigami.Units.smallSpacing
        objectName: "pageDescription"
        visible: header.description !== ""
        color: Kirigami.Theme.disabledTextColor
        font.italic: true
        text: header.description
        wrapMode: Text.WordWrap
    }
}
