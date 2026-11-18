// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2

// Whether the app looks for a newer version, and a button to look now.
ColumnsPage {
    id: updatesPage

    required property Updates updates

    title: qsTr("Updates")
    description: qsTr("Choose whether the app looks for a newer version, or look right now.")

    SectionHeader {
        title: qsTr("Looking for updates")
    }

    Sheet {
        SwitchRow {
            objectName: "checkForUpdatesSwitch"
            text: qsTr("Check for updates when the app starts")
            description: qsTr("Asks GitHub once a day while it runs. Nothing is installed without you saying so.")
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
