// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The places an item could go, with what's worn in each and what changes if
// the item takes its place.
ColumnLayout {
    id: table

    property list<reportPlacement> placements
    property bool colorblind: false

    spacing: Kirigami.Units.smallSpacing

    QQC2.Label {
        Layout.fillWidth: true
        opacity: 0.7
        font: Kirigami.Theme.smallFont
        text: qsTr("If you wear it in")
    }

    Repeater {
        model: table.placements

        RowLayout {
            id: place

            required property reportPlacement modelData

            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 0

                RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Label {
                        font.bold: place.modelData.best
                        text: place.modelData.slot
                    }

                    // the place its verdict is for
                    TonePill {
                        visible: place.modelData.best
                        text: qsTr("Best")
                        tone: Tone.Good
                        colorblind: table.colorblind
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    font: Kirigami.Theme.smallFont
                    text: place.modelData.wearing
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: 0

                Repeater {
                    model: place.modelData.changes

                    ToneLabel {
                        required property reportFigure modelData

                        Layout.alignment: Qt.AlignRight
                        font.bold: true
                        font.features: ({"tnum": 1})
                        text: modelData.change + " " + modelData.label
                        tone: modelData.tone
                        colorblind: table.colorblind
                    }
                }
            }
        }
    }
}
