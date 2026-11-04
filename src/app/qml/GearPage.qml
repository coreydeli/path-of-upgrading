// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// What the character you play wears, which items are judged against.
FormCard.FormCardPage {
    id: gearPage

    required property App app
    readonly property GearList gear: app.gearList
    readonly property Resistances resists: app.resistances

    title: qsTr("Your Gear")
    // how wide a card gets before it stops growing with the window
    readonly property real fullWidth: Kirigami.Units.gridUnit * 70

    // One of them, typed in off the character sheet.
    component ResistField: NumberField {
        visible: gearPage.resists.fromSheet
        minimum: -200
        maximum: 500
    }

    // A slot, which takes itself off when asked.
    component GearSlot: SlotRow {
        onTakeOffAsked: gearPage.gear.takeOff(slot)
    }

    FormCard.FormHeader {
        maximumWidth: gearPage.fullWidth
        title: gearPage.gear.character !== "" ? qsTr("Worn by %1").arg(gearPage.gear.character) : qsTr("Worn")
    }

    FormCard.FormCard {
        maximumWidth: gearPage.fullWidth

        FormCard.FormTextDelegate {
            id: overall
            objectName: "overallScore"
            visible: gearPage.gear.overall !== ""
            text: qsTr("Overall gear score %1 %2").arg(gearPage.gear.overall).arg(gearPage.gear.overallGrade)
            description: gearPage.gear.focus !== "" ? qsTr("Upgrade first: %1\n%2").arg(gearPage.gear.weakest).arg(gearPage.gear.focus) : qsTr("Upgrade first: %1").arg(gearPage.gear.weakest)
        }

        FormCard.FormTextDelegate {
            id: nobody
            objectName: "nobodyText"
            visible: gearPage.gear.character === ""
            text: qsTr("You play nobody yet.")
            description: qsTr("A character is made for you when you change something here. Or add one, or import a build, on the Characters page.")
        }

        FormCard.FormDelegateSeparator {
            visible: overall.visible || nobody.visible
        }

        Repeater {
            model: gearPage.gear

            delegate: GearSlot {
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
        Layout.maximumWidth: gearPage.fullWidth
        objectName: "gearHelp"
        text: qsTr("To save what you wear, hover each item you have on in the game and press the hotkey, then pick its slot under Wear as in the popup. What you save takes the place of what came with the build.")
    }

    FormCard.FormHeader {
        maximumWidth: gearPage.fullWidth
        title: qsTr("Your Resistances")
    }

    FormCard.FormCard {
        maximumWidth: gearPage.fullWidth
        id: resistances
        objectName: "resistances"

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

        SwitchRow {
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
        maximumWidth: gearPage.fullWidth
        visible: extras.visible
        title: qsTr("Weapon Swap, Flasks, Charms and Jewels")
    }

    FormCard.FormCard {
        maximumWidth: gearPage.fullWidth
        id: extras
        objectName: "extras"
        visible: gearPage.gear.hasExtras

        Repeater {
            model: gearPage.gear

            delegate: GearSlot {
                required property bool extra

                visible: extra
            }
        }
    }
}
