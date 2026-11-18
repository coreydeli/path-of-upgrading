// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Asks before everything the app keeps is wiped. Path of Building, its builds
// and the game aren't touched.
FormCard.FormCardDialog {
    id: dialog

    required property StartOver startOver

    onAccepted: startOver.run()

    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 30)
    title: qsTr("Start Over")
    standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel

    Component.onCompleted: {
        const wipe = standardButton(QQC2.Dialog.Ok);
        wipe.text = qsTr("Wipe It All");
        standardButton(QQC2.Dialog.Cancel).text = qsTr("Cancel");
    }

    FormCard.FormTextDelegate {
        text: qsTr("This removes your settings, your characters and imported builds, your gear, weights and item history, the saved site data and downloads, your session ID in the keyring, the autostart entry, and the app's hotkeys on your desktop. It can't be undone.")
        textItem.wrapMode: Text.Wrap
    }

    FormCard.FormTextDelegate {
        text: qsTr("Path of Building, its builds and the game are left alone. The app opens again on the setup guide.")
        textItem.wrapMode: Text.Wrap
    }
}
