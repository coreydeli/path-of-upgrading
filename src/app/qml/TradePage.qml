// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Whether the popup searches the trade site, in which league, for what
// you'll pay, and whether you're signed in to it.
ColumnsPage {
    id: tradePage

    required property TradeOptions options
    required property TradeSession session

    // the steps for finding the session ID
    property bool showFindSession: false
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
    description: qsTr("Searching the trade site from the popup, the league and what you will pay.")

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        SwitchRow {
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
            visible: tradePage.options.unavailable !== ""
            text: tradePage.options.unavailable
            textItem.wrapMode: Text.Wrap
        }
    }

    SectionHeader {
        title: qsTr("League")
    }

    LeagueCard {
        options: tradePage.options

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

    SectionHeader {
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

    SectionHeader {
        title: qsTr("Budget")
    }

    FormCard.FormCard {
        NumberField {
            objectName: "budgetMin"
            label: qsTr("Lowest price")
            description: qsTr("Listings cheaper than this are left out.")
            noneText: qsTr("Any")
            decimals: 1
            step: tradePage.options.currency === "divine" ? 0.5 : 1
            value: tradePage.options.budgetMin
            onEdited: value => tradePage.options.budgetMin = value
        }

        FormCard.FormDelegateSeparator {}

        NumberField {
            objectName: "budgetMax"
            label: qsTr("Highest price")
            description: qsTr("Listings that cost more are left out.")
            noneText: qsTr("Any")
            decimals: 1
            step: tradePage.options.currency === "divine" ? 0.5 : 1
            value: tradePage.options.budgetMax
            onEdited: value => tradePage.options.budgetMax = value
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormComboBoxDelegate {
            objectName: "currencyBox"
            text: qsTr("Currency")
            description: qsTr("What the lowest and highest price are counted in.")
            textRole: "text"
            valueRole: "value"
            model: tradePage.currencies
            currentIndex: tradePage.currencies.findIndex(currency => currency.value === tradePage.options.currency)
            onActivated: tradePage.options.currency = currentValue
        }

        FormCard.FormDelegateSeparator {}

        SwitchRow {
            objectName: "autoRateSwitch"
            text: qsTr("Use poe.ninja's currency prices")
            description: tradePage.options.ninjaRate > 0 ? qsTr("poe.ninja is a site that tracks what currency trades for in each league. Its price for a Divine Orb turns your budget into one currency. Right now a Divine Orb is %1 Exalted Orbs in %2.").arg(Math.round(tradePage.options.ninjaRate)).arg(tradePage.options.searchedLeague) : qsTr("poe.ninja is a site that tracks what currency trades for in each league. Its price for a Divine Orb turns your budget into one currency. It has no price yet.")
            checked: tradePage.options.autoRate
            onToggled: tradePage.options.autoRate = checked
        }

        FormCard.FormDelegateSeparator {}

        NumberField {
            objectName: "rateMinutes"
            label: qsTr("Update poe.ninja's prices automatically")
            description: tradePage.options.ratesStatus === "" ? qsTr("Asks poe.ninja again after this many minutes, from 1 to 60.") : qsTr("Asks poe.ninja again after this many minutes, from 1 to 60. %1").arg(tradePage.options.ratesStatus)
            unit: tradePage.options.rateMinutes === 1 ? qsTr("minute") : qsTr("minutes")
            minimum: 1
            maximum: 60
            enabled: tradePage.options.autoRate
            value: tradePage.options.rateMinutes
            onEdited: value => tradePage.options.rateMinutes = value
        }

        FormCard.FormDelegateSeparator {}

        NumberField {
            objectName: "divineRate"
            label: qsTr("Exalted Orbs to a Divine Orb")
            description: qsTr("Used when poe.ninja's prices are off, or it has no price yet.")
            noneText: qsTr("Not set")
            decimals: 1
            // poe.ninja's price goes before it while there is one
            enabled: !tradePage.options.autoRate || tradePage.options.ninjaRate === 0
            value: tradePage.options.divineRate
            onEdited: value => tradePage.options.divineRate = value
        }
    }

    SectionHeader {
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
                highlighted: true
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

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "findSessionButton"
            text: tradePage.showFindSession ? qsTr("Hide Where to Find It") : qsTr("Where Do I Find It?")
            description: qsTr("It's a cookie in your browser, from signing in to the Path of Exile website.")
            icon.name: tradePage.showFindSession ? "arrow-up" : "arrow-down"
            onClicked: tradePage.showFindSession = !tradePage.showFindSession
        }

        FormCard.FormDelegateSeparator {
            visible: findSessionSteps.visible
        }

        FormCard.FormTextDelegate {
            id: findSessionSteps
            objectName: "findSessionSteps"
            visible: tradePage.showFindSession
            text: tradePage.session.findSteps.join("\n")
            description: tradePage.session.findNotes.join(" ")
            textItem.wrapMode: Text.Wrap
        }

        FormCard.FormDelegateSeparator {
            visible: signInButton.visible
        }

        FormCard.FormButtonDelegate {
            id: signInButton
            objectName: "signInButton"
            visible: tradePage.showFindSession
            text: qsTr("Open the Sign In Page")
            description: qsTr("The Path of Exile website, in your browser.")
            icon.name: "internet-web-browser"
            onClicked: tradePage.session.openSignIn()
        }
    }

    SectionText {
        objectName: "sessionHelp"
        text: qsTr("The session ID is the POESESSID cookie of a browser that's signed in to pathofexile.com. It's kept in your system keyring and only ever sent to the trade site. Signed in, the site takes bigger searches, and Path of Building can weigh upgrade searches for your build.")
    }
}
