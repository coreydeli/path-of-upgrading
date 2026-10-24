import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Whether the popup searches the trade site, in which league, and for what
// you'll pay.
FormCard.FormCardPage {
    id: tradePage

    required property TradeOptions options
    readonly property var sellers: [
        {
            value: "available",
            text: qsTr("Instant buyout, or in person while online")
        },
        {
            value: "securable",
            text: qsTr("Instant buyout only")
        },
        {
            value: "online",
            text: qsTr("In person, while online")
        },
        {
            value: "any",
            text: qsTr("Any, even offline")
        }
    ]
    readonly property var currencies: [
        {
            value: "divine",
            text: qsTr("Divine Orbs")
        },
        {
            value: "exalted",
            text: qsTr("Exalted Orbs")
        },
        {
            value: "chaos",
            text: qsTr("Chaos Orbs")
        }
    ]

    title: qsTr("Trade")

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormSwitchDelegate {
            objectName: "enabledSwitch"
            text: qsTr("Search the trade site")
            description: qsTr("From the popup, for upgrades and for items like the one you checked.")
            checked: tradePage.options.enabled
            onToggled: tradePage.options.enabled = checked
        }

        FormCard.FormDelegateSeparator {
            visible: unavailableText.visible
        }

        FormCard.FormTextDelegate {
            id: unavailableText
            objectName: "unavailableText"
            // the switch says when it's off
            visible: tradePage.options.enabled && tradePage.options.unavailable !== ""
            text: tradePage.options.unavailable
            textItem.wrapMode: Text.Wrap
        }
    }

    FormCard.FormHeader {
        title: qsTr("League")
    }

    FormCard.FormCard {
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
            ].concat(tradePage.options.leagues.map(league => ({
                        value: league,
                        text: league
                    })))
            currentIndex: tradePage.options.league === "" ? 0 : tradePage.options.leagues.indexOf(tradePage.options.league) + 1
            onActivated: tradePage.options.league = currentValue
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "hardcoreSwitch"
            text: qsTr("Hardcore")
            checked: tradePage.options.hardcore
            onToggled: tradePage.options.hardcore = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "soloSwitch"
            text: qsTr("Solo self-found")
            description: qsTr("There's no trading in solo self-found, so this turns trade searches off.")
            checked: tradePage.options.soloSelfFound
            onToggled: tradePage.options.soloSelfFound = checked
        }

        FormCard.FormDelegateSeparator {
            visible: searchedText.visible
        }

        FormCard.FormTextDelegate {
            id: searchedText
            objectName: "searchedText"
            visible: tradePage.options.unavailable === ""
            text: qsTr("Searches go to")
            description: tradePage.options.searchedLeague
        }
    }

    FormCard.FormHeader {
        title: qsTr("Listings")
    }

    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            objectName: "sellersBox"
            text: qsTr("Sellers")
            textRole: "text"
            valueRole: "value"
            model: tradePage.sellers
            currentIndex: tradePage.sellers.findIndex(sellers => sellers.value === tradePage.options.sellers)
            onActivated: tradePage.options.sellers = currentValue
        }
    }

    FormCard.FormHeader {
        title: qsTr("Budget")
    }

    FormCard.FormCard {
        NumberField {
            objectName: "budgetMin"
            label: qsTr("From")
            noneText: qsTr("Any")
            decimals: 1
            step: tradePage.options.currency === "divine" ? 0.5 : 1
            value: tradePage.options.budgetMin
            onEdited: value => tradePage.options.budgetMin = value
        }

        FormCard.FormDelegateSeparator {}

        NumberField {
            objectName: "budgetMax"
            label: qsTr("Up to")
            noneText: qsTr("Any")
            decimals: 1
            step: tradePage.options.currency === "divine" ? 0.5 : 1
            value: tradePage.options.budgetMax
            onEdited: value => tradePage.options.budgetMax = value
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormComboBoxDelegate {
            objectName: "currencyBox"
            text: qsTr("In")
            textRole: "text"
            valueRole: "value"
            model: tradePage.currencies
            currentIndex: tradePage.currencies.findIndex(currency => currency.value === tradePage.options.currency)
            onActivated: tradePage.options.currency = currentValue
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            objectName: "autoRateSwitch"
            text: qsTr("Go by poe.ninja's rate")
            description: tradePage.options.ninjaRate > 0 ? qsTr("A Divine Orb is %1 Exalted Orbs in %2.").arg(Math.round(tradePage.options.ninjaRate)).arg(tradePage.options.searchedLeague) : qsTr("It hasn't said yet, so it goes by yours.")
            checked: tradePage.options.autoRate
            onToggled: tradePage.options.autoRate = checked
        }

        FormCard.FormDelegateSeparator {}

        NumberField {
            objectName: "divineRate"
            label: qsTr("Exalted Orbs to a Divine Orb")
            noneText: qsTr("Not set")
            decimals: 1
            // poe.ninja's goes before it while there is one
            enabled: !tradePage.options.autoRate || tradePage.options.ninjaRate === 0
            value: tradePage.options.divineRate
            onEdited: value => tradePage.options.divineRate = value
        }
    }
}
