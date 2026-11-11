// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The slot picked on the equipment screen: what's in it, what would raise its
// score, what to look for in a better one, and a trade search for it.
Item {
    id: panel

    required property GearList gear
    required property RecommendedUpgrades upgrades

    // whether there's a slot to show yet
    readonly property bool picked: shown.count > 0
    readonly property real contentHeight: picked ? shown.itemAt(0).implicitHeight : hint.implicitHeight

    implicitWidth: Kirigami.Units.gridUnit * 20
    implicitHeight: contentHeight + 2 * Kirigami.Units.gridUnit

    Plate {
        anchors.fill: parent
        line: Style.frame(Kirigami.Theme)
        lineWidth: 1
    }

    // just the picked slot
    EquipmentList {
        id: chosenSlot

        sourceModel: panel.gear
        only: panel.upgrades.chosen
    }

    QQC2.Label {
        id: hint

        objectName: "panelHint"
        anchors {
            fill: parent
            margins: Kirigami.Units.gridUnit
        }
        visible: !panel.picked
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        color: Kirigami.Theme.disabledTextColor
        text: qsTr("Pick a slot to see what would improve it.")
    }

    Repeater {
        id: shown

        model: panel.upgrades.chosen !== "" ? chosenSlot : 0

        delegate: ColumnLayout {
            id: content

            required property string slot
            required property string name
            required property string base
            required property bool fromPob
            required property string rarity
            required property string score
            required property string grade
            required property var notes

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: Kirigami.Units.gridUnit
            }
            spacing: Kirigami.Units.largeSpacing
            objectName: "panelContent"

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        objectName: "panelSlot"
                        Layout.fillWidth: true
                        text: content.slot
                        color: Style.accent(Kirigami.Theme)
                        font: Style.sectionFont(Kirigami.Theme.defaultFont)
                        elide: Text.ElideRight
                        Accessible.role: Accessible.Heading
                    }

                    QQC2.Label {
                        objectName: "panelItem"
                        Layout.fillWidth: true
                        text: content.name !== "" ? content.name : qsTr("Nothing saved")
                        color: content.name !== "" ? Style.rarity(content.rarity, Kirigami.Theme) : Kirigami.Theme.disabledTextColor
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                        wrapMode: Text.Wrap
                    }

                    QQC2.Label {
                        objectName: "panelBase"
                        Layout.fillWidth: true
                        visible: text !== ""
                        text: {
                            const parts = [];
                            if (content.name !== "" && content.base !== "")
                                parts.push(content.base);
                            if (content.name !== "" && content.fromPob)
                                parts.push(qsTr("from Path of Building"));
                            return parts.join(", ");
                        }
                        color: Kirigami.Theme.disabledTextColor
                        font: Kirigami.Theme.smallFont
                        wrapMode: Text.Wrap
                    }
                }

                ScoreGauge {
                    objectName: "panelGauge"
                    Layout.alignment: Qt.AlignTop
                    visible: content.score !== ""
                    score: content.score
                    grade: content.grade
                    size: "medium"
                }
            }

            RowRule {}

            SlotNotes {
                objectName: "panelNotes"
                Layout.fillWidth: true
                visible: content.notes.length > 0
                lines: content.notes
            }

            RowLayout {
                Layout.fillWidth: true
                visible: lookFor.text !== ""
                spacing: Kirigami.Units.largeSpacing

                QQC2.Label {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                    text: qsTr("Look for")
                    color: Kirigami.Theme.disabledTextColor
                }

                QQC2.Label {
                    id: lookFor

                    objectName: "panelLookFor"
                    Layout.fillWidth: true
                    text: panel.upgrades.chosenLookFor
                    wrapMode: Text.Wrap
                }
            }

            TradeStatus {
                objectName: "panelTradeStatus"
                Layout.fillWidth: true
                trade: panel.upgrades.chosenTrade
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.Button {
                    objectName: "panelFind"
                    highlighted: true
                    visible: panel.upgrades.tradeAvailable && panel.upgrades.chosenWorn
                    enabled: !panel.upgrades.chosenTrade.busy
                    text: qsTr("Find an Upgrade")
                    icon.name: "edit-find"
                    onClicked: panel.upgrades.findChosen()
                }

                QQC2.Button {
                    objectName: "panelShowResults"
                    visible: panel.upgrades.chosenTrade.listed
                    text: qsTr("Show Results")
                    icon.name: "view-list-details"
                    onClicked: panel.upgrades.showChosenResults()
                }

                Item {
                    Layout.fillWidth: true
                }

                QQC2.Button {
                    objectName: "panelTakeOff"
                    visible: content.name !== ""
                    text: qsTr("Take Off")
                    icon.name: "list-remove"
                    Accessible.name: qsTr("Take Off %1").arg(content.slot)
                    onClicked: panel.gear.takeOff(content.slot)
                }
            }
        }
    }
}
