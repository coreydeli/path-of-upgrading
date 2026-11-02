// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// A card's row with a switch, which the whole row flips. Kirigami Addons
// 1.7's FormSwitchDelegate sets its own checked when its switch changes, which
// takes the binding off it, so it stopped following the setting.
FormCard.AbstractFormDelegate {
    id: row

    property string description

    checkable: true
    Accessible.role: Accessible.CheckBox
    Accessible.description: description

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                Layout.fillWidth: true
                text: row.text
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                color: row.enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                // the row has it
                Accessible.ignored: true
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: row.description !== ""
                text: row.description
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
                Accessible.ignored: true
            }
        }

        QQC2.Switch {
            objectName: "switch"
            // the row takes it
            focusPolicy: Qt.NoFocus
            checked: row.checked
            Accessible.ignored: true
            onToggled: {
                row.toggle();
                row.toggled();
            }
        }
    }
}
