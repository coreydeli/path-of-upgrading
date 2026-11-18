// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// What the gear score of the character you play goes by: your build, as Path
// of Building weighs it, and how much each of its stats counts.
ColumnsPage {
    id: scorePage

    required property GearScoring scoring

    // On a wide window the stats are a table, as on the weights page.
    readonly property real whereWidth: Kirigami.Units.gridUnit * 20
    readonly property real levelWidth: Kirigami.Units.gridUnit * 12

    title: qsTr("Gear Score")
    description: qsTr("How items are scored for your character, and how much each stat counts.")
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
        title: scorePage.scoring.character !== "" ? qsTr("For %1").arg(scorePage.scoring.character) : qsTr("Gear Score")
    }

    Sheet {
        FormCard.FormTextDelegate {
            objectName: "nobodyText"
            visible: scorePage.scoring.character === ""
            text: qsTr("You haven't picked a character yet.")
            description: qsTr("Change anything here and the app makes a character for you. You can also add one yourself, or import a build, under Gear Settings, Characters.")
        }

        FormCard.FormTextDelegate {
            objectName: "scoreState"
            text: scorePage.scoring.state
            description: scorePage.scoring.summary
        }

        RowRule {}

        FormCard.FormButtonDelegate {
            objectName: "weighAgainButton"
            text: scorePage.scoring.weighing ? qsTr("Weighing Your Build") : qsTr("Weigh Again")
            description: qsTr("Asks Path of Building to weigh your build again.")
            icon.name: "view-refresh"
            enabled: scorePage.scoring.canWeigh && !scorePage.scoring.weighing
            onClicked: scorePage.scoring.weighAgain()
        }

        RowRule {}

        SwitchRow {
            objectName: "weightsOnlySwitch"
            text: qsTr("Judge by my stat weights alone")
            description: qsTr("Skips the gear score. Items are judged by the weights on the Stat Weights page.")
            checked: scorePage.scoring.weightsOnly
            onToggled: scorePage.scoring.weightsOnly = checked
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
        text: qsTr("The gear score rates an item from 0 to 100 by how much of what your build wants in that slot it has. Below, you can change how much each stat adds, for this character only.")
    }

    SectionHeader {
        visible: stats.visible
        title: qsTr("How much each stat counts")
    }

    SectionText {
        objectName: "impactHelp"
        visible: stats.visible
        text: qsTr("Each stat's points are multiplied by this number. Normal (1x) is how your build has it, Not at all (0x) leaves the stat out and Three times (3x) triples it.")
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
            third: qsTr("How much")
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
            text: qsTr("Reset to Normal")
            icon.name: "edit-reset"
            enabled: scorePage.scoring.tweaked
            onClicked: scorePage.scoring.reset()
        }
    }
}
