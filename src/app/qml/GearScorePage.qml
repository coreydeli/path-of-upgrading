// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// What the gear score of the character you play goes by: a build profile,
// and how much each of its stats counts.
ColumnsPage {
    id: scorePage

    required property GearScoring scoring

    // On a wide window the stats are a table, as on the weights page.
    readonly property real whereWidth: Kirigami.Units.gridUnit * 20
    readonly property real levelWidth: Kirigami.Units.gridUnit * 12

    title: qsTr("Gear Grade")
    description: qsTr("What the gear score goes by, and how much each stat counts.")
    maxColumns: wide ? 1 : 3
    maxColumnWidth: Kirigami.Units.gridUnit * (wide ? 66 : 40)

    // A stat, with how much it counts beside it.
    component StatRow: FormRow {
        id: statRow

        required property string label
        required property string where
        required property int level

        objectName: "stat:" + label
        text: label
        description: where
        descriptionWidth: scorePage.wide ? scorePage.whereWidth : 0
        controlsWidth: scorePage.wide ? scorePage.levelWidth : 0

        QQC2.ComboBox {
            objectName: "levelBox"
            // all as wide, for them to line up
            implicitContentWidthPolicy: QQC2.ComboBox.WidestText
            model: scorePage.scoring.levels
            currentIndex: statRow.level
            Accessible.name: statRow.label
            onActivated: chosen => scorePage.scoring.setLevel(statRow.label, chosen)
        }
    }

    SectionHeader {
        title: scorePage.scoring.character !== "" ? qsTr("For %1").arg(scorePage.scoring.character) : qsTr("Build Profile")
    }

    Sheet {
        FormCard.FormTextDelegate {
            objectName: "nobodyText"
            visible: scorePage.scoring.character === ""
            text: qsTr("You play nobody yet.")
            description: qsTr("A character is made for you when you change something here. Or add one, or import a build, on the Characters page.")
        }

        FormCard.FormComboBoxDelegate {
            objectName: "profileBox"
            text: qsTr("Build profile")
            description: scorePage.scoring.summary
            model: scorePage.scoring.profiles
            currentIndex: scorePage.scoring.profile
            onActivated: chosen => scorePage.scoring.profile = chosen
        }

        RowRule {
            visible: scoreError.visible
        }

        FormCard.FormTextDelegate {
            id: scoreError
            objectName: "scoreError"
            visible: text !== ""
            text: scorePage.scoring.error
            textItem.wrapMode: Text.Wrap
            textItem.color: Kirigami.Theme.negativeTextColor
        }
    }

    SectionText {
        objectName: "scoreHelp"
        visible: stats.visible
        text: qsTr("The gear score rates an item from 0 to 100 by how much it has of what the profile wants in its slot. You can change how much a stat adds under Affix Impact below, for this character only.")
    }

    SectionHeader {
        visible: stats.visible
        title: qsTr("Affix Impact")
    }

    SectionText {
        objectName: "impactHelp"
        visible: stats.visible
        text: qsTr("Changes how much each stat adds to the gear score. The number is what the stat's points are multiplied by, so Normal (1x) is how the profile has it, Not at all (0x) leaves the stat out and Three times (3x) triples it.")
    }

    Sheet {
        id: stats
        objectName: "stats"
        visible: statRows.count > 0

        TableHeader {
            objectName: "statsHeader"
            visible: scorePage.wide
            first: qsTr("Stat")
            second: qsTr("Where it counts")
            third: qsTr("Impact")
            descriptionWidth: scorePage.whereWidth
            controlsWidth: scorePage.levelWidth
        }

        RowRule {
            visible: scorePage.wide
        }

        Repeater {
            id: statRows
            model: scorePage.scoring

            delegate: StatRow {}
        }

        RowRule {}

        FormCard.FormButtonDelegate {
            objectName: "resetButton"
            text: qsTr("Back to the Profile's Own")
            icon.name: "edit-reset"
            enabled: scorePage.scoring.tweaked
            onClicked: scorePage.scoring.reset()
        }
    }
}
