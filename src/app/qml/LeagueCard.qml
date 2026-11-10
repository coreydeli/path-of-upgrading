// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

// The league you play in, which the Trade page and, without trade, the
// Settings page have. Anything put inside goes under the switches.
FormCard.FormCard {
    id: card

    required property TradeOptions options
    // Without trade searches there's nothing to turn off in solo self-found.
    property bool tradeOffered: true

    FormCard.FormComboBoxDelegate {
        objectName: "leagueBox"
        text: qsTr("League")
        textRole: "text"
        valueRole: "value"
        model: [
            {
                value: "",
                text: qsTr("The current league")
            }
        ].concat(card.options.leagues.map(league => ({
                    value: league,
                    text: league
                })))
        currentIndex: card.options.league === "" ? 0 : card.options.leagues.indexOf(card.options.league) + 1
        onActivated: card.options.league = currentValue
    }

    FormCard.FormDelegateSeparator {}

    SwitchRow {
        objectName: "hardcoreSwitch"
        text: qsTr("Hardcore")
        checked: card.options.hardcore
        onToggled: card.options.hardcore = checked
    }

    FormCard.FormDelegateSeparator {}

    SwitchRow {
        objectName: "soloSwitch"
        text: qsTr("Solo self-found")
        description: card.tradeOffered ? qsTr("There's no trading in solo self-found, so this turns trade searches off.") : qsTr("For characters that play without trading.")
        checked: card.options.soloSelfFound
        onToggled: card.options.soloSelfFound = checked
    }
}
