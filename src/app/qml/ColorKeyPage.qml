// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What the colors in the app mean. Each sample is drawn with the same
// functions the app draws with, so it follows the look and the colorblind
// setting.
ColumnsPage {
    id: keyPage

    required property OverlayOptions options

    title: qsTr("Color Key")
    description: qsTr("What the colors in the app mean. They follow your look and your color settings.")

    SectionHeader {
        title: qsTr("Item names")
    }

    Sheet {
        ColorSample {
            objectName: "rarity:Normal"
            text: qsTr("Normal")
            description: qsTr("Grey, as in the game.")
            sample: qsTr("Normal")
            tint: Style.rarity("Normal", Kirigami.Theme)
        }

        RowRule {}

        ColorSample {
            objectName: "rarity:Magic"
            text: qsTr("Magic")
            description: qsTr("Blue.")
            sample: qsTr("Magic")
            tint: Style.rarity("Magic", Kirigami.Theme)
        }

        RowRule {}

        ColorSample {
            objectName: "rarity:Rare"
            text: qsTr("Rare")
            description: qsTr("Yellow.")
            sample: qsTr("Rare")
            tint: Style.rarity("Rare", Kirigami.Theme)
        }

        RowRule {}

        ColorSample {
            objectName: "rarity:Unique"
            text: qsTr("Unique")
            description: qsTr("Orange-brown.")
            sample: qsTr("Unique")
            tint: Style.rarity("Unique", Kirigami.Theme)
        }
    }

    SectionHeader {
        title: qsTr("Gear score grades")
    }

    Sheet {
        ColorSample {
            objectName: "grade:S"
            text: qsTr("Grade S")
            description: qsTr("The best, in the accent color.")
            sample: "S"
            tint: Style.grade("S", Kirigami.Theme)
        }

        RowRule {}

        ColorSample {
            objectName: "grade:A"
            text: qsTr("Grade A")
            description: qsTr("Good, in green.")
            sample: "A"
            tint: Style.grade("A", Kirigami.Theme)
        }

        RowRule {}

        ColorSample {
            objectName: "grade:B"
            text: qsTr("Grade B")
            description: qsTr("Middling, in plain text.")
            sample: "B"
            tint: Style.grade("B", Kirigami.Theme)
        }

        RowRule {}

        ColorSample {
            objectName: "grade:C"
            text: qsTr("Grade C")
            description: qsTr("Weak, in yellow or orange.")
            sample: "C"
            tint: Style.grade("C", Kirigami.Theme)
        }

        RowRule {}

        ColorSample {
            objectName: "grade:D"
            text: qsTr("Grade D")
            description: qsTr("The worst, greyed out.")
            sample: "D"
            tint: Style.grade("D", Kirigami.Theme)
        }
    }

    SectionText {
        text: qsTr("The letter and the gauge beside it share the grade's color.")
    }

    SectionHeader {
        title: qsTr("Under an item")
    }

    Sheet {
        ColorSample {
            objectName: "mark:corrupted"
            text: qsTr("Corrupted")
            description: qsTr("In red, under the item's name.")
            sample: qsTr("Corrupted")
            tint: Kirigami.Theme.negativeTextColor
        }

        RowRule {}

        ColorSample {
            objectName: "mark:fractured"
            text: qsTr("Fractured")
            description: qsTr("In yellow or orange, under the item's name.")
            sample: qsTr("Fractured")
            tint: Kirigami.Theme.neutralTextColor
        }
    }

    SectionHeader {
        title: qsTr("Gear tiles")
    }

    Sheet {
        FormRow {
            objectName: "frameKey"
            text: qsTr("Picked slot")
            description: qsTr("A tile's frame takes the accent color when you pick the slot, point at it or tab to it.")

            leading: Rectangle {
                objectName: "frameSample"
                Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2
                color: "transparent"
                border.width: 2
                border.color: Style.accent(Kirigami.Theme)
                Accessible.role: Accessible.StaticText
                Accessible.name: qsTr("Picked slot")
            }
        }
    }

    SectionHeader {
        title: qsTr("Stat chips")
    }

    Sheet {
        Repeater {
            model: Style.kinded ? [
                { key: "resistance", text: qsTr("Resistance"), what: qsTr("Green."), sample: qsTr("+30% Fire Resistance") },
                { key: "damage", text: qsTr("Damage"), what: qsTr("Red."), sample: qsTr("+20% Spell Damage") },
                { key: "defence", text: qsTr("Defence"), what: qsTr("Blue: Life, Energy Shield, Armour, Evasion and the like."), sample: qsTr("+50 Maximum Life") },
                { key: "other", text: qsTr("Anything else"), what: qsTr("Grey."), sample: qsTr("+10 Dexterity") }
            ] : []

            delegate: FormRow {
                id: chipRow

                required property var modelData

                objectName: "chipKey:" + modelData.key
                text: modelData.text
                description: modelData.what

                leading: StatChip {
                    objectName: "chipSample"
                    text: chipRow.modelData.sample
                    Accessible.name: chipRow.modelData.text
                }
            }
        }

        FormRow {
            objectName: "chipsOneColor"
            visible: !Style.kinded
            text: qsTr("One color")
            description: qsTr("In this look the chips are all the same color. Pocket colors them by what the stat is for.")
        }
    }

    SectionHeader {
        title: qsTr("Better and worse")
    }

    Sheet {
        ColorSample {
            objectName: "tone:good"
            text: qsTr("Better")
            description: qsTr("In the popup and the changes table.")
            sample: qsTr("Better")
            tint: Style.tone(Tone.Good, keyPage.options.colorblind, Kirigami.Theme)
        }

        RowRule {}

        ColorSample {
            objectName: "tone:bad"
            text: qsTr("Worse")
            description: qsTr("It gets worse for you.")
            sample: qsTr("Worse")
            tint: Style.tone(Tone.Bad, keyPage.options.colorblind, Kirigami.Theme)
        }

        RowRule {}

        ColorSample {
            objectName: "tone:warning"
            text: qsTr("Better, with a catch")
            description: qsTr("Better, but with a downside.")
            sample: qsTr("Careful")
            tint: Style.tone(Tone.Warning, keyPage.options.colorblind, Kirigami.Theme)
        }
    }

    SectionText {
        text: qsTr("\"Colorblind-friendly colors\" on the General page swaps green and red for blue and orange.")
    }

    SectionHeader {
        title: qsTr("Red text")
    }

    Sheet {
        ColorSample {
            objectName: "error"
            text: qsTr("Something went wrong")
            description: qsTr("Red text in a card says what failed.")
            sample: qsTr("Failed")
            tint: Kirigami.Theme.negativeTextColor
        }
    }
}
