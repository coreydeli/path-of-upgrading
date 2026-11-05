// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Layouts
import org.kde.kirigamiaddons.formcard as FormCard

// The setup guide's league: the same settings as the Trade page.
ColumnLayout {
    id: step

    required property TradeOptions options

    spacing: 0

    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            objectName: "setupLeagueBox"
            text: qsTr("League")
            textRole: "text"
            valueRole: "value"
            model: [
                {
                    value: "",
                    text: qsTr("The current league")
                }
            ].concat(step.options.leagues.map(league => ({
                        value: league,
                        text: league
                    })))
            currentIndex: step.options.league === "" ? 0 : step.options.leagues.indexOf(step.options.league) + 1
            onActivated: step.options.league = currentValue
        }

        FormCard.FormDelegateSeparator {}

        SwitchRow {
            objectName: "setupHardcoreSwitch"
            text: qsTr("Hardcore")
            checked: step.options.hardcore
            onToggled: step.options.hardcore = checked
        }

        FormCard.FormDelegateSeparator {}

        SwitchRow {
            objectName: "setupSoloSwitch"
            text: qsTr("Solo self-found")
            description: qsTr("There's no trading in solo self-found, so this turns trade searches off.")
            checked: step.options.soloSelfFound
            onToggled: step.options.soloSelfFound = checked
        }
    }
}
