pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// What the character you play wears, which items are judged against.
FormCard.FormCardPage {
    id: gearPage

    required property App app
    readonly property GearList gear: app.gearList

    title: qsTr("Your Gear")

    // A slot and what's in it.
    component SlotRow: FormCard.FormTextDelegate {
        id: slotRow

        required property string slot
        required property string name
        required property string base
        required property bool fromPob
        required property string score
        required property string grade

        objectName: "slot:" + slot
        text: slot
        description: {
            if (name === "")
                return qsTr("Nothing saved");
            const parts = [name];
            if (base !== "")
                parts.push(base);
            if (fromPob)
                parts.push(qsTr("from Path of Building"));
            return parts.join(", ");
        }

        trailing: RowLayout {
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                objectName: "gearScore"
                visible: slotRow.score !== ""
                text: qsTr("Gear score %1 %2").arg(slotRow.score).arg(slotRow.grade)
            }

            QQC2.ToolButton {
                objectName: "takeOffButton"
                visible: slotRow.name !== ""
                text: qsTr("Take Off")
                icon.name: "list-remove"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: gearPage.gear.takeOff(slotRow.slot)
            }
        }
    }

    FormCard.FormHeader {
        title: gearPage.gear.character !== "" ? qsTr("Worn by %1").arg(gearPage.gear.character) : qsTr("Worn")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            objectName: "nobodyText"
            visible: gearPage.gear.character === ""
            text: qsTr("You play nobody yet.")
            description: qsTr("Add a character or import a build on the Characters page.")
        }

        Repeater {
            model: gearPage.gear

            delegate: SlotRow {
                required property bool extra

                visible: !extra
            }
        }

        FormCard.FormDelegateSeparator {
            visible: gearError.visible
        }

        FormCard.FormTextDelegate {
            id: gearError
            objectName: "gearError"
            visible: text !== ""
            text: gearPage.gear.error
            textItem.wrapMode: Text.Wrap
            textItem.color: Kirigami.Theme.negativeTextColor
        }
    }

    FormCard.FormSectionText {
        objectName: "gearHelp"
        visible: gearPage.gear.character !== ""
        text: qsTr("To save what you wear, hover each item you have on in the game and press the hotkey, then pick its slot under Wear as in the popup. What you save takes the place of what came with the build.")
    }

    FormCard.FormHeader {
        visible: extras.visible
        title: qsTr("Weapon Swap, Flasks, Charms and Jewels")
    }

    FormCard.FormCard {
        id: extras
        objectName: "extras"
        visible: gearPage.gear.hasExtras

        Repeater {
            model: gearPage.gear

            delegate: SlotRow {
                required property bool extra

                visible: extra
            }
        }
    }
}
