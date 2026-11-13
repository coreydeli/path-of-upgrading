// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The title over a card, a small slab of paper. It stands in for FormHeader,
// and takes the same title and maximumWidth, which ColumnsPage sets.
Item {
    id: header

    property string title
    // The column of a page with columns to have what follows start in, or -1
    // to let ColumnsPage put it in the shortest.
    property int column: -1
    // as FormHeader's: the width the card above it is held to
    property real maximumWidth: Kirigami.Units.gridUnit * 30

    readonly property real inset: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
    readonly property bool restricted: width > maximumWidth
    readonly property real sideMargin: restricted ? Math.round((width - maximumWidth) / 2) + Kirigami.Units.smallSpacing : inset

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing

    ColumnLayout {
        id: content

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: header.sideMargin
            rightMargin: header.sideMargin
            bottomMargin: Kirigami.Units.smallSpacing
        }
        spacing: 2

        Rectangle {
            Layout.maximumWidth: content.width
            implicitWidth: slab.implicitWidth + 2 * Kirigami.Units.smallSpacing + 2
            implicitHeight: slab.implicitHeight + 2
            color: Style.paper
            Accessible.role: Accessible.Heading
            Accessible.name: header.title

            Text {
                id: slab

                anchors.centerIn: parent
                width: Math.min(implicitWidth, parent.width - 2 * Kirigami.Units.smallSpacing)
                elide: Text.ElideRight
                font: Style.sectionFont(Kirigami.Theme.defaultFont)
                color: Style.ink
                text: header.title
            }
        }
    }
}
