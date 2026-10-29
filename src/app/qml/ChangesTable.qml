pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What equipping an item changes on your character, in columns of what you
// have now, what you'd have with it and the change.
ColumnLayout {
    id: table

    property list<reportFigure> figures
    property bool colorblind: false

    spacing: 0

    // the widest a number gets, which the change is in bold
    TextMetrics {
        id: number

        font.bold: true
        text: "+88,888%"
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing
        opacity: 0.7

        QQC2.Label {
            Layout.fillWidth: true
            font: Kirigami.Theme.smallFont
            text: qsTr("If you equip it")
        }

        QQC2.Label {
            Layout.preferredWidth: number.width
            horizontalAlignment: Text.AlignRight
            font: Kirigami.Theme.smallFont
            text: qsTr("Now")
        }

        QQC2.Label {
            Layout.preferredWidth: number.width
            horizontalAlignment: Text.AlignRight
            font: Kirigami.Theme.smallFont
            text: qsTr("With it")
        }

        Item {
            Layout.preferredWidth: number.width
        }
    }

    Repeater {
        model: table.figures

        RowLayout {
            id: change

            required property reportFigure modelData

            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                QQC2.Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: change.modelData.label
                }

                // "under the cap"
                ToneLabel {
                    Layout.fillWidth: true
                    visible: text !== ""
                    font: Kirigami.Theme.smallFont
                    text: change.modelData.note
                    tone: change.modelData.tone
                    colorblind: table.colorblind
                }
            }

            QQC2.Label {
                Layout.preferredWidth: number.width
                Layout.alignment: Qt.AlignTop
                horizontalAlignment: Text.AlignRight
                opacity: 0.7
                text: change.modelData.from
            }

            QQC2.Label {
                Layout.preferredWidth: number.width
                Layout.alignment: Qt.AlignTop
                horizontalAlignment: Text.AlignRight
                text: change.modelData.to
            }

            ToneLabel {
                Layout.preferredWidth: number.width
                Layout.alignment: Qt.AlignTop
                horizontalAlignment: Text.AlignRight
                font.bold: true
                text: change.modelData.change
                tone: change.modelData.tone
                colorblind: table.colorblind
            }
        }
    }
}
