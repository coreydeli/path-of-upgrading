// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Shows an update being taken: the download, then either the install or the
// command to run for it. It is its own dialog, as Update Now closes the first.
FormCard.FormCardDialog {
    id: dialog

    required property App app
    readonly property int stage: app.updates.stage
    readonly property bool busy: stage === UpdateStage.Installing

    onStageChanged: {
        if (stage !== UpdateStage.Idle && !opened)
            open();
        else if (stage === UpdateStage.Idle && opened)
            close();
    }
    // Closing it stops a download and clears what it said
    onClosed: app.updates.dismiss()

    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 30)
    title: stage === UpdateStage.Failed ? qsTr("The Update Didn't Work") : qsTr("Updating Path of Upgrading")
    standardButtons: QQC2.Dialog.Close
    closePolicy: busy ? QQC2.Popup.NoAutoClose : QQC2.Popup.CloseOnEscape

    Component.onCompleted: {
        const close = standardButton(QQC2.Dialog.Close);
        // Cancel while it downloads, and nothing while it installs
        close.text = Qt.binding(() => stage === UpdateStage.Downloading ? qsTr("Cancel") : qsTr("Close"));
        close.enabled = Qt.binding(() => !busy);
        if (stage !== UpdateStage.Idle)
            open();
    }

    FormCard.FormTextDelegate {
        text: {
            switch (dialog.stage) {
            case UpdateStage.Downloading:
                return qsTr("Downloading version %1 and checking it.").arg(dialog.app.updates.newVersion);
            case UpdateStage.Installing:
                return qsTr("Installing version %1.").arg(dialog.app.updates.newVersion);
            case UpdateStage.Manual:
                return (dialog.app.updates.installError.length > 0 ? dialog.app.updates.installError + " " : "")
                    + qsTr("Version %1 is downloaded and checked. Run this in a terminal to install it, then start the app again.").arg(dialog.app.updates.newVersion);
            case UpdateStage.Installed:
                return qsTr("Version %1 is installed. Restart the app to use it.").arg(dialog.app.updates.newVersion);
            case UpdateStage.Failed:
                return dialog.app.updates.installError;
            }
            return "";
        }
        textItem.wrapMode: Text.Wrap
    }

    QQC2.ProgressBar {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        visible: dialog.stage === UpdateStage.Downloading || dialog.stage === UpdateStage.Installing
        indeterminate: dialog.stage === UpdateStage.Installing || dialog.app.updates.progress < 0
        value: Math.max(0, dialog.app.updates.progress)
    }

    QQC2.TextArea {
        objectName: "installCommand"
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        visible: dialog.stage === UpdateStage.Manual && text.length > 0
        text: dialog.app.updates.command
        readOnly: true
        wrapMode: TextEdit.WrapAnywhere
        // Kirigami 6.13 has no fixedWidthFont, and this only shows on Linux
        font.family: "monospace"
    }

    FormCard.FormButtonDelegate {
        objectName: "restartButton"
        visible: dialog.stage === UpdateStage.Installed
        text: qsTr("Restart Now")
        icon.name: "view-refresh"
        onClicked: dialog.app.updates.restart()
    }

    FormCard.FormButtonDelegate {
        objectName: "copyCommandButton"
        visible: dialog.stage === UpdateStage.Manual && dialog.app.updates.command.length > 0
        text: qsTr("Copy the Command")
        icon.name: "edit-copy"
        onClicked: dialog.app.updates.copyCommand()
    }

    FormCard.FormButtonDelegate {
        objectName: "openFolderButton"
        visible: dialog.stage === UpdateStage.Manual
        text: qsTr("Open the Folder")
        icon.name: "folder-open"
        onClicked: dialog.app.updates.openFolder()
    }
}
