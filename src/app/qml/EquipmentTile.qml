// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

// A slot on the gear page's equipment screen: its name, what's worn in it in
// the item's color and its gear score. An empty slot is a dashed frame that
// says so. Picking it is a click, or Enter or Space once Tab has got to it.
QQC2.AbstractButton {
    id: tile

    required property string slot
    required property string name
    required property string base
    required property bool fromPob
    required property string rarity
    required property string score
    required property string grade

    // the one the panel beside the screen is showing
    property bool chosen: false

    readonly property bool empty: name === ""
    // what's in it in one line, for the screen reader and the tests
    readonly property string description: {
        if (empty)
            return qsTr("Nothing saved");
        const parts = [name];
        if (base !== "")
            parts.push(base);
        if (fromPob)
            parts.push(qsTr("from Path of Building"));
        return parts.join(", ");
    }
    // the frame is lit when it's the one picked, or it's about to be
    readonly property bool lit: chosen || hovered || visualFocus
    readonly property color edge: lit ? Style.gold(Kirigami.Theme) : Style.frame(Kirigami.Theme)

    objectName: "tile:" + slot
    implicitWidth: Kirigami.Units.gridUnit * 10
    implicitHeight: Kirigami.Units.gridUnit * 4.4
    padding: Kirigami.Units.largeSpacing
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: slot
    Accessible.description: description

    background: Item {
        // what's worn is in a frame
        Rectangle {
            anchors.fill: parent
            visible: !tile.empty
            color: tile.chosen ? Style.tint(Kirigami.Theme.backgroundColor, Style.gold(Kirigami.Theme), 0.12)
                               : tile.hovered ? Style.tint(Kirigami.Theme.backgroundColor, Style.gold(Kirigami.Theme), 0.06)
                                              : Kirigami.Theme.backgroundColor
            border {
                width: 1
                color: tile.edge
            }
        }

        // the item's color down the left, as on a result
        Rectangle {
            visible: !tile.empty
            width: 3
            height: parent.height
            color: Style.rarity(tile.rarity, Kirigami.Theme)
        }

        // an empty one is dashed
        Shape {
            anchors.fill: parent
            visible: tile.empty
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: tile.chosen ? Style.tint(Kirigami.Theme.backgroundColor, Style.gold(Kirigami.Theme), 0.08)
                                       : "transparent"
                strokeColor: tile.edge
                strokeWidth: 1
                strokeStyle: ShapePath.DashLine
                dashPattern: [4, 3]
                startX: 0.5
                startY: 0.5

                PathLine {
                    x: tile.width - 0.5
                    y: 0.5
                }

                PathLine {
                    x: tile.width - 0.5
                    y: tile.height - 0.5
                }

                PathLine {
                    x: 0.5
                    y: tile.height - 0.5
                }

                PathLine {
                    x: 0.5
                    y: 0.5
                }
            }
        }

        // where the keyboard is
        Rectangle {
            anchors {
                fill: parent
                margins: -2
            }
            visible: tile.visualFocus
            color: "transparent"
            border {
                width: 1
                color: Kirigami.Theme.highlightColor
            }
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            QQC2.Label {
                objectName: "slotName"
                Layout.fillWidth: true
                text: tile.slot
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
                Accessible.ignored: true
            }

            QQC2.Label {
                objectName: "itemName"
                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: tile.empty ? Text.AlignHCenter : Text.AlignLeft
                text: tile.empty ? qsTr("Empty") : tile.name
                color: tile.empty ? Kirigami.Theme.disabledTextColor : Style.rarity(tile.rarity, Kirigami.Theme)
                font.italic: tile.empty
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                Accessible.ignored: true
            }
        }

        ScoreGauge {
            objectName: "tileGauge"
            Layout.alignment: Qt.AlignVCenter
            visible: tile.score !== ""
            score: tile.score
            grade: tile.grade
            size: "small"
        }
    }
}
