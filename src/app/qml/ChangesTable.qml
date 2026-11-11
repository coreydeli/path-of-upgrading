// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What equipping an item changes, in columns of what you have now, what you'd
// have with it and the change.
ColumnLayout {
    id: table

    property string title: qsTr("If you equip it")
    property list<reportFigure> figures
    property bool colorblind: false
    // for notes that say how a stat was counted, not how its change reads
    property bool plainNotes: false
    // in whole pixels, as Qt 6.8's layouts round it down and the widest
    // number wouldn't fit
    readonly property int numberWidth: Math.ceil(number.implicitWidth)
    // the biggest change shown, which the bars are measured against
    readonly property real biggest: figures.reduce((most, figure) => Math.max(most, sizeOf(figure.change)), 0)

    spacing: 0

    // "+1,234%" is 1234
    function sizeOf(change: string): real {
        const size = Math.abs(parseFloat(change.replace(/,/g, "")));
        return isNaN(size) ? 0 : size;
    }

    // The widest a number gets, in bold like the change, so the columns line
    // up. It's a label like theirs, as the style can draw labels wider than
    // TextMetrics measures.
    QQC2.Label {
        id: number

        visible: false
        font: Style.figureFont(Kirigami.Theme.defaultFont, 0.9)
        text: "+88,888%"
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing
        opacity: 0.7

        QQC2.Label {
            Layout.fillWidth: true
            font: Style.labelFont(Kirigami.Theme.smallFont)
            text: table.title
        }

        QQC2.Label {
            Layout.preferredWidth: table.numberWidth
            horizontalAlignment: Text.AlignRight
            font: Style.labelFont(Kirigami.Theme.smallFont)
            text: qsTr("Now")
        }

        QQC2.Label {
            Layout.preferredWidth: table.numberWidth
            horizontalAlignment: Text.AlignRight
            font: Style.labelFont(Kirigami.Theme.smallFont)
            text: qsTr("With it")
        }

        Item {
            Layout.preferredWidth: table.numberWidth
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
                    wrapMode: Text.Wrap
                    text: change.modelData.label
                }

                // "under the cap"
                ToneLabel {
                    Layout.fillWidth: true
                    visible: text !== ""
                    wrapMode: Text.Wrap
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.italic: true
                    text: change.modelData.note
                    tone: table.plainNotes ? Tone.Plain : change.modelData.tone
                    colorblind: table.colorblind
                }
            }

            QQC2.Label {
                Layout.preferredWidth: table.numberWidth
                Layout.alignment: Qt.AlignTop
                horizontalAlignment: Text.AlignRight
                opacity: 0.7
                font.features: ({"tnum": 1})
                text: change.modelData.from
            }

            QQC2.Label {
                Layout.preferredWidth: table.numberWidth
                Layout.alignment: Qt.AlignTop
                horizontalAlignment: Text.AlignRight
                font.features: ({"tnum": 1})
                text: change.modelData.to
            }

            Item {
                Layout.preferredWidth: table.numberWidth
                Layout.preferredHeight: figure.implicitHeight + 6
                Layout.alignment: Qt.AlignTop

                ToneLabel {
                    id: figure

                    width: parent.width
                    horizontalAlignment: Text.AlignRight
                    font: Style.figureFont(Kirigami.Theme.defaultFont, 0.9)
                    text: change.modelData.change
                    tone: change.modelData.tone
                    colorblind: table.colorblind
                }

                // how big the change is, under its number, longer for more
                Rectangle {
                    readonly property real part: table.biggest > 0 ? table.sizeOf(change.modelData.change) / table.biggest : 0

                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: Math.max(3, part * parent.width * 0.8)
                    height: 3
                    visible: change.modelData.tone === Tone.Good || change.modelData.tone === Tone.Bad
                    color: Style.tone(change.modelData.tone, table.colorblind, Kirigami.Theme)
                }
            }
        }
    }
}
