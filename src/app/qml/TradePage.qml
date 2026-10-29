import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Whether the popup searches the trade site, in which league, for what
// you'll pay, and whether you're signed in to it.
FormCard.FormCardPage {
    id: tradePage

    required property TradeOptions options
    required property TradeSession session
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
            currentIndex: tradePage.sellers.findIndex(seller => seller.value === tradePage.options.sellers)
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

    FormCard.FormHeader {
        title: qsTr("Signed In")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            objectName: "sessionStatus"
            text: tradePage.session.status
            textItem.wrapMode: Text.Wrap
        }

        FormCard.FormDelegateSeparator {}

        FormRow {
            text: qsTr("Session ID")
            enabled: !tradePage.session.busy

            QQC2.TextField {
                id: sessionField
                objectName: "sessionField"
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Paste it here")
                Accessible.name: qsTr("Session ID")
                onAccepted: saveButton.clicked()

                // it isn't kept in the page once it's saved
                Connections {
                    target: tradePage.session

                    function onStored() {
                        sessionField.clear();
                    }
                }
            }

            QQC2.Button {
                id: saveButton
                objectName: "saveButton"
                text: qsTr("Save")
                enabled: sessionField.text.trim() !== ""
                onClicked: tradePage.session.save(sessionField.text)
            }

            QQC2.Button {
                objectName: "forgetButton"
                text: qsTr("Forget")
                enabled: tradePage.session.saved
                onClicked: tradePage.session.forget()
            }
        }

        FormCard.FormDelegateSeparator {
            visible: sessionError.visible
        }

        FormCard.FormTextDelegate {
            id: sessionError
            objectName: "sessionError"
            visible: text !== ""
            text: tradePage.session.error
            textItem.wrapMode: Text.Wrap
            textItem.color: Kirigami.Theme.negativeTextColor
        }
    }

    FormCard.FormSectionText {
        objectName: "sessionHelp"
        text: qsTr("The session ID is the POESESSID cookie of a browser that's signed in to pathofexile.com. It's kept in your system keyring and only ever sent to the trade site. Signed in, the site takes bigger searches.")
    }
}
