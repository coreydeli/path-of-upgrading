// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Says a newer version is out, with what's new in it. It opens once the
// window is up, and not over the setup guide.
FormCard.FormCardDialog {
    id: dialog

    required property App app
    // it waits for the window, which isn't up when it starts in the background
    property bool windowShown: false

    readonly property bool wanted: windowShown && app.updates.pending && !app.onboarding.pending

    // Hiding the window leaves it open, for when the window is back.
    onWantedChanged: {
        if (wanted && !opened)
            open();
    }
    // Escape and Later both leave it for the next start
    onClosed: app.updates.later()
    onAccepted: app.updates.updateNow()
    onDiscarded: app.updates.skip()

    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 30)
    title: qsTr("A New Version Is Out")
    standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel | QQC2.Dialog.Discard
    closePolicy: QQC2.Popup.CloseOnEscape

    Component.onCompleted: {
        const update = standardButton(QQC2.Dialog.Ok);
        update.text = qsTr("Update Now");
        update.icon.name = "download";
        standardButton(QQC2.Dialog.Cancel).text = qsTr("Later");
        standardButton(QQC2.Dialog.Discard).text = qsTr("Skip This Version");
        if (wanted)
            open();
    }

    FormCard.FormTextDelegate {
        text: qsTr("Version %1 of Path of Upgrading is out. You have %2.").arg(dialog.app.updates.newVersion).arg(dialog.app.updates.version)
        textItem.wrapMode: Text.Wrap
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(notes.implicitHeight, dialog.parent.height * 0.4)
        contentWidth: availableWidth
        clip: true
        visible: notes.text.length > 0
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        QQC2.Label {
            id: notes

            width: parent.width
            text: dialog.app.updates.notes
            wrapMode: Text.Wrap
        }
    }
}
