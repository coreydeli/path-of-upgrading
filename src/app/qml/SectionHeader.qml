// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The title over a card, in the heading font and the gold, with a short rule
// under it. It stands in for FormHeader, and takes the same title and
// maximumWidth, which ColumnsPage sets.
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

        QQC2.Label {
            Layout.fillWidth: true
            color: Style.accent(Kirigami.Theme)
            font: Style.sectionFont(Kirigami.Theme.defaultFont)
            text: header.title
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.Heading
        }

        Ornament {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 5
            diamond: !Style.plain
        }
    }
}
