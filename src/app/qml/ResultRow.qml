pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One listing in the results window, see TradeResults for what it has.
Item {
    id: row

    required property string name
    required property string base
    required property string price
    required property string converted
    required property string account
    required property bool online
    required property bool instantBuyout
    required property string listed
    required property var mods
    required property resultVerdict verdict
    property bool colorblind: false

    width: ListView.view.width
    implicitHeight: content.implicitHeight + 2 * Kirigami.Units.smallSpacing

    ColumnLayout {
        id: content

        anchors {
            fill: parent
            topMargin: Kirigami.Units.smallSpacing
            bottomMargin: Kirigami.Units.smallSpacing
            // clear of the scroll bar
            rightMargin: Kirigami.Units.largeSpacing
        }
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 0

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.bold: true
                    text: row.name
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    text: row.base
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: 0

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    font.bold: true
                    text: row.price
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    visible: text !== ""
                    opacity: 0.7
                    font: Kirigami.Theme.smallFont
                    text: row.converted
                }
            }
        }

        // what it does for the character you play
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
            visible: row.verdict.verdict !== ""
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                ToneLabel {
                    font.bold: true
                    text: row.verdict.verdict
                    tone: row.verdict.tone
                    colorblind: row.colorblind
                }

                ToneLabel {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: row.verdict.score
                    tone: row.verdict.tone
                    colorblind: row.colorblind
                }
            }

            Flow {
                Layout.fillWidth: true
                visible: row.verdict.figures.length > 0
                spacing: Kirigami.Units.largeSpacing

                Repeater {
                    model: row.verdict.figures

                    ToneLabel {
                        required property reportFigure modelData

                        font: Kirigami.Theme.smallFont
                        text: modelData.label + " " + modelData.change
                        tone: modelData.tone
                        colorblind: row.colorblind
                    }
                }
            }

            Repeater {
                model: row.verdict.losses

                ToneLabel {
                    required property string modelData

                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font: Kirigami.Theme.smallFont
                    text: modelData
                    tone: Tone.Bad
                    colorblind: row.colorblind
                }
            }

            Repeater {
                model: row.verdict.risks

                ToneLabel {
                    required property string modelData

                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font: Kirigami.Theme.smallFont
                    text: modelData
                    tone: Tone.Warning
                    colorblind: row.colorblind
                }
            }

            ToneLabel {
                Layout.fillWidth: true
                visible: text !== ""
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
                text: row.verdict.caps
                tone: Tone.Good
                colorblind: row.colorblind
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: text !== ""
                wrapMode: Text.Wrap
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                text: row.verdict.checking ? qsTr("Checking with Path of Building") : row.verdict.pobError
            }
        }

        Repeater {
            model: row.mods

            QQC2.Label {
                required property resultMod modelData

                Layout.fillWidth: true
                wrapMode: Text.Wrap
                // those that come with the base are less to go by
                opacity: ["enchant", "rune", "implicit"].includes(modelData.tag) ? 0.7 : 1
                text: modelData.text
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                text: row.account
            }

            // an instant buyout is bought whether the seller is there or not
            ToneLabel {
                readonly property bool reachable: row.instantBuyout || row.online

                font: Kirigami.Theme.smallFont
                text: row.instantBuyout ? qsTr("Instant buyout") : row.online ? qsTr("Online") : qsTr("Offline")
                tone: reachable ? Tone.Good : Tone.Plain
                opacity: reachable ? 1 : 0.7
                colorblind: row.colorblind
            }

            QQC2.Label {
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                text: row.listed
            }
        }
    }

    Kirigami.Separator {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
    }
}
