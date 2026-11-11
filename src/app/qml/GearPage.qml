// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// What the character you play wears, which items are judged against.
ColumnsPage {
    id: gearPage

    required property App app
    readonly property GearList gear: app.gearList
    readonly property Resistances resists: app.resistances

    // On a wide window the slots are laid out like the character's equipment
    // screen, which wants the page to itself. Narrower, they're a list.
    readonly property bool equipment: wide

    title: qsTr("Your Gear")
    description: qsTr("What your character wears, and where an upgrade would help most.")
    // the slots need room to put their notes beside them
    minColumnWidth: Kirigami.Units.gridUnit * 44
    maxColumnWidth: Kirigami.Units.gridUnit * (equipment ? 66 : 60)
    maxColumns: equipment ? 1 : 3

    // One of them, typed in off the character sheet.
    component ResistField: NumberField {
        visible: gearPage.resists.fromSheet
        minimum: -200
        maximum: 500
    }

    // The most one resistance can be.
    component MaximumField: NumberField {
        visible: gearPage.resists.known
        minimum: 1
        maximum: 100
    }

    // A slot, which takes itself off when asked.
    component GearSlot: SlotRow {
        onTakeOffAsked: gearPage.gear.takeOff(slot)
    }

    SectionHeader {
        visible: recommended.visible
        title: qsTr("Recommended Upgrades")
    }

    RecommendationCard {
        id: recommended
        upgrades: gearPage.app.recommendedUpgrades
    }

    SectionText {
        visible: recommended.visible
        text: qsTr("The slots where a better item helps most, and the stats to look for in it. Empty slots come first, then the lowest gear scores.")
    }

    SectionHeader {
        title: gearPage.gear.character !== "" ? qsTr("Worn by %1").arg(gearPage.gear.character) : qsTr("Worn")
    }

    Sheet {
        FormCard.FormTextDelegate {
            id: overall
            objectName: "overallScore"
            visible: gearPage.gear.overall !== ""
            text: qsTr("Overall gear score %1 %2").arg(gearPage.gear.overall).arg(gearPage.gear.overallGrade)
            // the headline of the page
            textItem.font: Style.titleFont(Kirigami.Theme.defaultFont, 1.3)
            textItem.color: Style.grade(gearPage.gear.overallGrade, Kirigami.Theme)
        }

        FormCard.FormTextDelegate {
            id: nobody
            objectName: "nobodyText"
            visible: gearPage.gear.character === ""
            text: qsTr("You play nobody yet.")
            description: qsTr("A character is made for you when you change something here. Or add one, or import a build, on the Characters page.")
        }

        RowRule {
            visible: overall.visible || nobody.visible
        }

        EquipmentBoard {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.gridUnit
            visible: gearPage.equipment
            gear: gearPage.gear
            upgrades: gearPage.app.recommendedUpgrades
        }

        Repeater {
            model: gearPage.gear

            delegate: GearSlot {
                required property bool extra

                visible: !extra && !gearPage.equipment
            }
        }

        RowRule {
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

    SectionText {
        objectName: "gearHelp"
        text: qsTr("To save what you wear, hover each item you have on in the game and press the hotkey, then pick its slot under Wear as in the popup. What you save takes the place of what came with the build.")
    }

    SectionHeader {
        objectName: "resistancesHeader"
        title: qsTr("Resistances")
    }

    Sheet {
        id: resistances
        objectName: "resistances"

        FormCard.FormTextDelegate {
            objectName: "resistSource"
            text: gearPage.resists.sourceNote
            textItem.wrapMode: Text.Wrap
        }

        RowRule {}

        FormCard.FormTextDelegate {
            id: resistSummary
            objectName: "resistSummary"
            visible: !gearPage.resists.fromSheet && gearPage.resists.known
            text: gearPage.resists.summary
            textItem.wrapMode: Text.Wrap
        }

        RowRule {
            visible: resistSummary.visible
        }

        SwitchRow {
            objectName: "fromSheetSwitch"
            text: qsTr("Type them in from your character sheet")
            description: qsTr("Use the numbers on the game's character sheet in place of your build's, as it shows them, over the maximum too. They follow your gear from then on.")
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

        RowRule {
            visible: fireMaximum.visible
        }

        MaximumField {
            id: fireMaximum
            objectName: "fireMaximum"
            label: qsTr("Fire maximum")
            description: qsTr("The most each resistance can be, 75 unless your passive tree raises it. What your gear adds is counted for you, and your build's are used until you change one.")
            value: gearPage.resists.fireMaximum
            onEdited: value => gearPage.resists.fireMaximum = value
        }

        MaximumField {
            objectName: "coldMaximum"
            label: qsTr("Cold maximum")
            value: gearPage.resists.coldMaximum
            onEdited: value => gearPage.resists.coldMaximum = value
        }

        MaximumField {
            objectName: "lightningMaximum"
            label: qsTr("Lightning maximum")
            value: gearPage.resists.lightningMaximum
            onEdited: value => gearPage.resists.lightningMaximum = value
        }

        MaximumField {
            objectName: "chaosMaximum"
            label: qsTr("Chaos maximum")
            value: gearPage.resists.chaosMaximum
            onEdited: value => gearPage.resists.chaosMaximum = value
        }

        NumberField {
            objectName: "resistMargin"
            visible: gearPage.resists.known
            label: qsTr("Safety buffer")
            description: qsTr("How far over the maximum you want each resistance to stay. Some of what counts towards them, like a charm, isn't always on, so an item that leaves you closer than this gets a warning.")
            maximum: 100
            value: gearPage.resists.margin
            onEdited: value => gearPage.resists.margin = value
        }

        RowRule {
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

    SectionHeader {
        visible: extras.visible
        title: qsTr("Weapon Swap, Flasks, Charms and Jewels")
    }

    // on the equipment screen they're under the rest
    Sheet {
        id: extras
        objectName: "extras"
        visible: gearPage.gear.hasExtras && !gearPage.equipment

        Repeater {
            model: gearPage.gear

            delegate: GearSlot {
                required property bool extra

                visible: extra
            }
        }
    }
}
