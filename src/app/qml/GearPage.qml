pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// What the character you play wears, which items are judged against.
FormCard.FormCardPage {
    id: gearPage

    required property App app
    readonly property GearList gear: app.gearList
    readonly property Resistances resists: app.resistances

    title: qsTr("Your Gear")

    // One of them, typed in off the character sheet.
    component ResistField: NumberField {
        visible: gearPage.resists.fromSheet
        minimum: -200
        maximum: 500
    }

    // A slot and what's in it.
    component SlotRow: FormRow {
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
            Accessible.name: qsTr("Take Off %1").arg(slotRow.slot)
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: gearPage.gear.takeOff(slotRow.slot)
        }
    }

    FormCard.FormHeader {
        title: gearPage.gear.character !== "" ? qsTr("Worn by %1").arg(gearPage.gear.character) : qsTr("Worn")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            id: overall
            objectName: "overallScore"
            visible: gearPage.gear.overall !== ""
            text: qsTr("Overall gear score %1 %2").arg(gearPage.gear.overall).arg(gearPage.gear.overallGrade)
            description: qsTr("Upgrade first: %1").arg(gearPage.gear.weakest)
        }

        FormCard.FormDelegateSeparator {
            visible: overall.visible
        }

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
        visible: resistances.visible
        title: qsTr("Your Resistances")
    }

    FormCard.FormCard {
        id: resistances
        objectName: "resistances"
        visible: gearPage.gear.character !== ""

        FormCard.FormTextDelegate {
            id: resistSummary
            objectName: "resistSummary"
            visible: !gearPage.resists.fromSheet
            text: gearPage.resists.known ? gearPage.resists.summary : qsTr("Not known yet")
            description: gearPage.resists.known ? qsTr("From your build in Path of Building, with what you wear now.") : qsTr("Import a build, or type them in off your character sheet.")
            textItem.wrapMode: Text.Wrap
        }

        FormCard.FormDelegateSeparator {
            visible: resistSummary.visible
        }

        FormCard.FormSwitchDelegate {
            objectName: "fromSheetSwitch"
            text: qsTr("Typed in off your character sheet")
            description: qsTr("As the sheet shows them, over the maximum too. They follow your gear from then on.")
            checked: gearPage.resists.fromSheet
            onToggled: gearPage.resists.fromSheet = checked
        }

        ResistField {
            objectName: "fireResist"
            label: qsTr("Fire")
            value: gearPage.resists.fire
            onEdited: value => gearPage.resists.fire = value
        }

        ResistField {
            objectName: "coldResist"
            label: qsTr("Cold")
            value: gearPage.resists.cold
            onEdited: value => gearPage.resists.cold = value
        }

        ResistField {
            objectName: "lightningResist"
            label: qsTr("Lightning")
            value: gearPage.resists.lightning
            onEdited: value => gearPage.resists.lightning = value
        }

        ResistField {
            objectName: "chaosResist"
            label: qsTr("Chaos")
            value: gearPage.resists.chaos
            onEdited: value => gearPage.resists.chaos = value
        }

        FormCard.FormDelegateSeparator {
            visible: maximumResist.visible
        }

        NumberField {
            id: maximumResist
            objectName: "maximumResist"
            visible: gearPage.resists.known
            label: qsTr("Maximum resistance")
            description: qsTr("75 unless your passive tree raises it. What your gear adds is counted for you.")
            minimum: 1
            maximum: 100
            value: gearPage.resists.maximum
            onEdited: value => gearPage.resists.maximum = value
        }

        NumberField {
            objectName: "resistMargin"
            visible: gearPage.resists.known
            label: qsTr("Margin over the maximum")
            description: qsTr("How far over it a resistance has to be to count as capped, for what isn't always on, like a charm.")
            maximum: 100
            value: gearPage.resists.margin
            onEdited: value => gearPage.resists.margin = value
        }

        FormCard.FormDelegateSeparator {
            visible: resistError.visible
        }

        FormCard.FormTextDelegate {
            id: resistError
            objectName: "resistError"
            visible: text !== ""
            text: gearPage.resists.error
            textItem.wrapMode: Text.Wrap
            textItem.color: Kirigami.Theme.negativeTextColor
        }
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
