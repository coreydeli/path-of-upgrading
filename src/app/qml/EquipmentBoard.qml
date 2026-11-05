// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What the character wears, laid out like its equipment screen: the helmet at
// the top with the amulet beside it, a weapon either side of the body armour,
// and so on down to the boots, with the flasks, charms and the rest in a row
// underneath. Picking a slot shows it in a panel beside them. Tab goes
// through the slots in the order they read across the screen.
RowLayout {
    id: board

    required property GearList gear
    required property RecommendedUpgrades upgrades

    // a slot, with the panel following the one that's picked
    component Slot: EquipmentTile {
        chosen: board.upgrades.chosen === slot
        onClicked: board.upgrades.chosen = slot
    }

    objectName: "equipment"
    spacing: Kirigami.Units.gridUnit

    ColumnLayout {
        Layout.alignment: Qt.AlignTop
        spacing: Kirigami.Units.largeSpacing

        GridLayout {
            objectName: "equipmentScreen"
            columns: 3
            columnSpacing: Kirigami.Units.smallSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            Repeater {
                model: EquipmentList {
                    sourceModel: board.gear
                }

                delegate: Slot {
                    required property int cellRow
                    required property int cellColumn

                    Layout.row: cellRow
                    Layout.column: cellColumn
                    Layout.fillWidth: true
                }
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: others.count > 0
            text: qsTr("Weapon Swap, Flasks, Charms and Jewels")
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            wrapMode: Text.Wrap
        }

        Flow {
            objectName: "equipmentOthers"
            Layout.fillWidth: true
            visible: others.count > 0
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                id: others

                model: EquipmentList {
                    sourceModel: board.gear
                    extras: true
                }

                delegate: Slot {
                    implicitWidth: Kirigami.Units.gridUnit * 9
                    implicitHeight: Kirigami.Units.gridUnit * 3.6
                }
            }
        }
    }

    SlotPanel {
        objectName: "slotPanel"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: Kirigami.Units.gridUnit * 18
        gear: board.gear
        upgrades: board.upgrades
    }
}
