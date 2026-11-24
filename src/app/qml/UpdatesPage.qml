// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2

// Whether the app looks for a newer version, and a button to look now.
ColumnsPage {
    id: updatesPage

    required property Updates updates

    title: qsTr("Updates")
    description: qsTr("Choose whether the app checks for a newer version, or check right now.")

    SectionHeader {
        title: qsTr("Looking for updates")
    }

    Sheet {
        SwitchRow {
            objectName: "checkForUpdatesSwitch"
            text: qsTr("Check for updates when the app starts")
            description: qsTr("Asks GitHub once a day while it runs.")
            checked: updatesPage.updates.checkOnLaunch
            onToggled: updatesPage.updates.checkOnLaunch = checked
        }

        RowRule {}

        SwitchRow {
            objectName: "includeBetasSwitch"
            text: qsTr("Include beta builds")
            description: qsTr("Betas come out often and may have bugs.")
            checked: updatesPage.updates.includeBetas
            onToggled: updatesPage.updates.includeBetas = checked
        }

        RowRule {}

        SwitchRow {
            objectName: "installByItselfSwitch"
            text: qsTr("Install updates for me")
            description: qsTr("Don't want to be asked? The app downloads a new version, installs it and starts it again. Installs that ask for your password wait for you.")
            checked: updatesPage.updates.installByItself
            onToggled: updatesPage.updates.installByItself = checked
        }

        RowRule {}

        FormRow {
            objectName: "readyRow"
            visible: updatesPage.updates.stage === UpdateStage.Ready
            text: qsTr("Version %1 is ready").arg(updatesPage.updates.newVersion)
            description: qsTr("It's downloaded and checked.")

            QQC2.Button {
                objectName: "installReadyButton"
                text: qsTr("Install")
                onClicked: updatesPage.updates.updateNow()
            }
        }

        RowRule {
            visible: updatesPage.updates.stage === UpdateStage.Ready
        }

        FormRow {
            text: qsTr("You have version %1").arg(updatesPage.updates.version)
            description: updatesPage.updates.status

            QQC2.Button {
                objectName: "checkNowButton"
                text: qsTr("Check Now")
                enabled: !updatesPage.updates.checking
                onClicked: updatesPage.updates.checkNow()
            }
        }
    }
}
