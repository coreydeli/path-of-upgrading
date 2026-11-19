// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The mods of a worn item under the headings the game puts them in. A
// fractured one says so.
ColumnLayout {
    id: mods

    // gearList's mods role: { group, text, fractured, starts }
    property var lines: []

    spacing: 0

    Repeater {
        model: mods.lines

        delegate: ColumnLayout {
            id: mod

            required property var modelData

            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                objectName: "modGroup"
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                visible: mod.modelData.starts
                text: mod.modelData.group
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                Accessible.role: Accessible.Heading
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    objectName: "modText"
                    Layout.fillWidth: true
                    text: mod.modelData.text
                    wrapMode: Text.Wrap
                    Accessible.name: mod.modelData.fractured ? qsTr("%1, fractured").arg(text) : text
                }

                QQC2.Label {
                    objectName: "modFractured"
                    visible: mod.modelData.fractured
                    text: qsTr("Fractured")
                    color: Kirigami.Theme.neutralTextColor
                    font: Kirigami.Theme.smallFont
                    Accessible.ignored: true
                }
            }
        }
    }
}
