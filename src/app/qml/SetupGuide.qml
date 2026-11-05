// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Goes through what a new user sets first: the league, trade searches, a
// character, and the key to press over an item. It opens on the first run, or
// with --setup, once the window is up. Skipping it counts as done.
FormCard.FormCardDialog {
    id: guide

    required property App app
    // it waits for the window, which isn't up when it starts in the background
    property bool windowShown: false

    property string step: "welcome"
    // There's no trading in solo self-found, so the trade step is left out.
    readonly property var steps: app.tradeOptions.soloSelfFound ? ["welcome", "league", "character", "done"] : ["welcome", "league", "trade", "character", "done"]
    readonly property bool wanted: windowShown && app.onboarding.pending

    function begin(): void {
        step = "welcome";
        open();
    }

    function next(): void {
        const index = steps.indexOf(step);
        if (index < steps.length - 1)
            step = steps[index + 1];
        else
            finish();
    }

    function back(): void {
        const index = steps.indexOf(step);
        if (index > 0)
            step = steps[index - 1];
    }

    // for Skip as well as the last step
    function finish(): void {
        app.onboarding.finish();
        close();
    }

    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 30)
    title: {
        switch (step) {
        case "league":
            return qsTr("Your League");
        case "trade":
            return qsTr("Trade Searches");
        case "character":
            return qsTr("Your Character");
        case "done":
            return qsTr("You're All Set");
        default:
            return qsTr("Welcome to Path of Upgrading");
        }
    }
    closePolicy: QQC2.Popup.CloseOnEscape

    // Hiding the window leaves it open, for when the window is back.
    onWantedChanged: {
        if (wanted && !opened)
            begin();
    }
    // Escape skips it
    onClosed: app.onboarding.finish()
    Component.onCompleted: {
        if (wanted)
            begin();
    }

    QQC2.ScrollView {
        id: scroll

        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(stepColumn.implicitHeight, guide.parent.height * 0.6)
        contentWidth: availableWidth
        clip: true
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        ColumnLayout {
            id: stepColumn

            width: scroll.availableWidth
            spacing: 0

            ColumnLayout {
                objectName: "welcomeStep"
                visible: guide.step === "welcome"
                Layout.fillWidth: true
                spacing: 0

                FormCard.FormCard {
                    FormCard.FormTextDelegate {
                        text: qsTr("Hold the mouse over an item in Path of Exile 2 and press a key. A popup says whether it's an upgrade for your build.")
                        textItem.wrapMode: Text.Wrap
                    }

                    FormCard.FormDelegateSeparator {}

                    FormCard.FormTextDelegate {
                        text: qsTr("This takes a minute, and you can change all of it later.")
                        textItem.wrapMode: Text.Wrap
                    }
                }
            }

            SetupLeagueStep {
                objectName: "leagueStep"
                visible: guide.step === "league"
                Layout.fillWidth: true
                options: guide.app.tradeOptions
            }

            SetupTradeStep {
                objectName: "tradeStep"
                visible: guide.step === "trade"
                Layout.fillWidth: true
                options: guide.app.tradeOptions
                session: guide.app.tradeSession
            }

            SetupCharacterStep {
                objectName: "characterStep"
                visible: guide.step === "character"
                Layout.fillWidth: true
                characters: guide.app.characterList
                importer: guide.app.buildImporter
            }

            ColumnLayout {
                objectName: "doneStep"
                visible: guide.step === "done"
                Layout.fillWidth: true
                spacing: 0

                FormCard.FormCard {
                    FormCard.FormTextDelegate {
                        objectName: "hotkeyText"
                        text: guide.app.onboarding.hotkey !== "" ? qsTr("Hold the mouse over an item in the game and press %1.").arg(guide.app.onboarding.hotkey) : qsTr("The key to check an item isn't set. Choose one on the Settings page.")
                        textItem.wrapMode: Text.Wrap
                    }

                    FormCard.FormDelegateSeparator {}

                    FormCard.FormTextDelegate {
                        text: qsTr("To see this guide again, start the program with --setup.")
                        textItem.wrapMode: Text.Wrap
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing

        QQC2.Button {
            objectName: "skipButton"
            visible: guide.step !== "done"
            text: qsTr("Skip")
            onClicked: guide.finish()
        }

        Item {
            Layout.fillWidth: true
        }

        QQC2.Button {
            objectName: "backButton"
            visible: guide.step !== "welcome"
            text: qsTr("Back")
            onClicked: guide.back()
        }

        QQC2.Button {
            objectName: "nextButton"
            text: guide.step === "welcome" ? qsTr("Set It Up") : guide.step === "done" ? qsTr("Finish") : qsTr("Next")
            highlighted: true
            onClicked: guide.next()
        }
    }
}
