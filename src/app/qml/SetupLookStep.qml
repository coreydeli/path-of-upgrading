// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The setup guide's look: a swatch for each, which saves the same choice as
// the Appearance page's.
ColumnLayout {
    id: step

    required property Appearance appearance

    spacing: 0

    Sheet {
        FormCard.FormTextDelegate {
            text: qsTr("The app has more than one look. You can change it later on the Appearance page.")
            textItem.wrapMode: Text.Wrap
        }

        RowRule {}

        GridLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.largeSpacing

            Repeater {
                model: step.appearance.designs

                delegate: QQC2.AbstractButton {
                    id: tile

                    required property var modelData
                    readonly property bool current: step.appearance.design === modelData.id

                    objectName: "lookSwatch:" + modelData.id
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    padding: Kirigami.Units.largeSpacing
                    hoverEnabled: true
                    text: modelData.name
                    onClicked: step.appearance.design = modelData.id

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.name
                    Accessible.description: modelData.note
                    Accessible.checked: current

                    background: Rectangle {
                        radius: Style.shapeSmall
                        color: tile.hovered ? Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.06) : "transparent"
                        border.width: tile.current ? 2 : 1
                        border.color: tile.current ? Kirigami.Theme.highlightColor : Style.frame(Kirigami.Theme)
                    }

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: tile.modelData.name
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Row {
                            spacing: Kirigami.Units.smallSpacing

                            Repeater {
                                model: tile.modelData.swatch

                                delegate: Rectangle {
                                    required property string modelData

                                    width: Kirigami.Units.gridUnit
                                    height: Kirigami.Units.gridUnit
                                    color: modelData
                                    border.width: 1
                                    border.color: Style.inner(Kirigami.Theme)
                                }
                            }
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: tile.modelData.note
                            font: Kirigami.Theme.smallFont
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }
}
