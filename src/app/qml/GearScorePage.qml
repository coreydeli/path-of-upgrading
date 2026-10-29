pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// What the gear score of the character you play goes by: a build profile,
// and how much each of its stats counts.
FormCard.FormCardPage {
    id: scorePage

    required property GearScoring scoring

    title: qsTr("Gear Score")

    // A stat, with how much it counts beside it.
    component StatRow: FormRow {
        id: statRow

        required property string label
        required property string where
        required property int level

        objectName: "stat:" + label
        text: label
        description: where

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

    FormCard.FormHeader {
        title: scorePage.scoring.character !== "" ? qsTr("For %1").arg(scorePage.scoring.character) : qsTr("Build Profile")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            objectName: "nobodyText"
            visible: scorePage.scoring.character === ""
            text: qsTr("You play nobody yet.")
            description: qsTr("Add a character or import a build on the Characters page.")
        }

        FormCard.FormComboBoxDelegate {
            objectName: "profileBox"
            visible: scorePage.scoring.character !== ""
            text: qsTr("Build profile")
            description: scorePage.scoring.summary
            model: scorePage.scoring.profiles
            currentIndex: scorePage.scoring.profile
            onActivated: chosen => scorePage.scoring.profile = chosen
        }

        FormCard.FormDelegateSeparator {
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

    FormCard.FormSectionText {
        objectName: "scoreHelp"
        visible: stats.visible
        text: qsTr("The gear score rates an item from 0 to 100 by how much it has of what the profile wants in its slot. You can make a stat count for more or less below, for this character only.")
    }

    FormCard.FormHeader {
        visible: stats.visible
        title: qsTr("How Much Each Stat Counts")
    }

    FormCard.FormCard {
        id: stats
        objectName: "stats"
        visible: statRows.count > 0

        Repeater {
            id: statRows
            model: scorePage.scoring

            delegate: StatRow {}
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "resetButton"
            text: qsTr("Back to the Profile's Own")
            icon.name: "edit-reset"
            enabled: scorePage.scoring.tweaked
            onClicked: scorePage.scoring.reset()
        }
    }
}
