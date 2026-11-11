// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// An item from the history, with what it has on it, beside the list. It
// slides down with the page as it scrolls, so it stays in view next to a long
// list.
Item {
    id: panel

    // what CheckedItems::details() gives
    property var details: ({})
    // the page's scrolling, for the panel to follow
    property Flickable scrolling: null
    // how far from the top of its place the panel is now
    readonly property real offset: {
        if (!scrolling)
            return 0;
        const top = panel.mapToItem(scrolling.contentItem, 0, 0).y;
        return Math.max(0, Math.min(scrolling.contentY - top + Kirigami.Units.largeSpacing, panel.height - card.height));
    }
    readonly property bool picked: details.name !== undefined

    signal checkAgainAsked

    // as tall as the list, for it to have room to slide in
    property real room: 0

    implicitHeight: Math.max(card.height, room)

    Item {
        id: card

        y: panel.offset
        width: parent.width
        height: content.implicitHeight + 2 * Kirigami.Units.gridUnit

        Plate {
            anchors.fill: parent
            line: Style.frame(Kirigami.Theme)
            lineWidth: 1
        }

        ColumnLayout {
            id: content

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: Kirigami.Units.gridUnit
            }
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                objectName: "detailName"
                Layout.fillWidth: true
                visible: panel.picked
                text: panel.details.name ?? ""
                color: Style.rarity(panel.details.rarity ?? "", Kirigami.Theme)
                font: Style.titleFont(Kirigami.Theme.defaultFont, 1.2)
                wrapMode: Text.Wrap
                Accessible.role: Accessible.Heading
            }

            QQC2.Label {
                objectName: "detailKind"
                Layout.fillWidth: true
                visible: text !== ""
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
                text: {
                    const parts = [];
                    for (const part of [panel.details.rarity, panel.details.base, panel.details.itemClass]) {
                        if (part)
                            parts.push(part);
                    }
                    return parts.join(", ");
                }
            }

            QQC2.Label {
                objectName: "detailLevels"
                Layout.fillWidth: true
                visible: text !== ""
                color: Kirigami.Theme.disabledTextColor
                text: {
                    const parts = [];
                    if (panel.details.itemLevel > 0)
                        parts.push(qsTr("Item level %1").arg(panel.details.itemLevel));
                    if (panel.details.requiredLevel > 0)
                        parts.push(qsTr("needs level %1").arg(panel.details.requiredLevel));
                    return parts.join(", ");
                }
            }

            QQC2.Label {
                objectName: "detailCorrupted"
                Layout.fillWidth: true
                visible: panel.details.corrupted === true
                color: Kirigami.Theme.negativeTextColor
                text: qsTr("Corrupted")
            }

            RowRule {
                visible: panel.picked
            }

            ColumnLayout {
                objectName: "detailProperties"
                Layout.fillWidth: true
                visible: properties.count > 0
                spacing: 0

                Repeater {
                    id: properties

                    model: panel.details.properties ?? []

                    delegate: QQC2.Label {
                        required property var modelData

                        Layout.fillWidth: true
                        text: modelData.text
                        wrapMode: Text.Wrap
                    }
                }
            }

            ColumnLayout {
                objectName: "detailMods"
                Layout.fillWidth: true
                visible: mods.count > 0
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    id: mods

                    model: panel.details.mods ?? []

                    delegate: RowLayout {
                        id: mod

                        required property var modelData

                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                        // what kind it is, unless it's just a mod, in a column
                        // of its own for the mods to line up
                        QQC2.Label {
                            Layout.alignment: Qt.AlignTop
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                            text: mod.modelData.tag === "explicit" ? "" : mod.modelData.tag.charAt(0).toUpperCase() + mod.modelData.tag.slice(1)
                            color: Kirigami.Theme.disabledTextColor
                            font: Kirigami.Theme.smallFont
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: mod.modelData.text
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: panel.picked

                Item {
                    Layout.fillWidth: true
                }

                QQC2.Button {
                    objectName: "detailCheckAgain"
                    text: qsTr("Check Again")
                    icon.name: "view-refresh"
                    onClicked: panel.checkAgainAsked()
                }
            }
        }
    }
}
