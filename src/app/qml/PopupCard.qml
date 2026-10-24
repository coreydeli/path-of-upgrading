import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What the popup shows: the item you checked and what it means for you.
Rectangle {
    id: card

    required property ItemPopup popup
    readonly property itemReport report: popup.report

    // the game's colours for them
    function rarityColor(rarity: string): color {
        switch (rarity) {
        case "Magic":
            return "#8888ff";
        case "Rare":
            return "#ffff77";
        case "Unique":
            return "#af6025";
        default:
            return Kirigami.Theme.textColor;
        }
    }

    // dark whatever the desktop's colours, like the game
    Kirigami.Theme.colorSet: Kirigami.Theme.Complementary
    Kirigami.Theme.inherit: false

    implicitWidth: Kirigami.Units.gridUnit * 22
    implicitHeight: content.implicitHeight + 2 * Kirigami.Units.largeSpacing
    color: Kirigami.Theme.backgroundColor
    radius: Kirigami.Units.cornerRadius
    border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor,
                                                          Kirigami.Theme.textColor, 0.2)

    HoverHandler {
        onHoveredChanged: card.popup.hovered = hovered
    }

    ColumnLayout {
        id: content

        anchors {
            fill: parent
            margins: Kirigami.Units.largeSpacing
        }
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 3
                wrapMode: Text.Wrap
                text: card.popup.message || card.report.name
                color: card.popup.message ? Kirigami.Theme.textColor : card.rarityColor(card.report.rarity)
            }

            QQC2.ToolButton {
                Layout.alignment: Qt.AlignTop
                icon.name: "window-close"
                text: qsTr("Close")
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: card.popup.close()
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            opacity: 0.7
            text: card.report.details.join(", ")
        }

        RowLayout {
            Layout.fillWidth: true
            visible: card.report.verdict !== ""
            spacing: Kirigami.Units.largeSpacing

            ToneLabel {
                font.bold: true
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                text: card.report.verdict
                tone: card.report.verdictTone
                colorblind: card.popup.colorblind
            }

            ToneLabel {
                Layout.fillWidth: true
                font.bold: true
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                text: card.report.score
                tone: card.report.verdictTone
                colorblind: card.popup.colorblind
            }
        }

        // what the score is and what it's against: "for your build, vs Grim Crown"
        QQC2.Label {
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            opacity: 0.7
            text: [card.report.scoreLabel, card.report.versus].filter(part => part !== "").join(", ")
        }

        // what it costs you, which matters more than what it gives
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: card.report.losses.length > 0
            type: Kirigami.MessageType.Error
            text: card.report.losses.join("\n")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: card.report.risks.length > 0
            type: Kirigami.MessageType.Warning
            text: card.report.risks.join("\n")
        }

        ToneLabel {
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            text: card.report.caps
            tone: Tone.Good
            colorblind: card.popup.colorblind
        }

        // what Path of Building says it does to your build
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: card.report.headline.length > 0
            spacing: Kirigami.Units.largeSpacing

            Repeater {
                model: card.report.headline

                ColumnLayout {
                    id: figure

                    required property reportFigure modelData

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    spacing: 0

                    ToneLabel {
                        font.bold: true
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                        text: figure.modelData.change
                        tone: figure.modelData.tone
                        colorblind: card.popup.colorblind
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        opacity: 0.7
                        font: Kirigami.Theme.smallFont
                        text: figure.modelData.label
                    }
                }
            }
        }

        // the item's stats that change the most of those that count
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: card.report.stats.length > 0
            spacing: 0

            Repeater {
                model: card.report.stats

                ToneLabel {
                    required property reportFigure modelData

                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: modelData.label
                    tone: modelData.tone
                    colorblind: card.popup.colorblind
                }
            }
        }

        // what equipping it changes on your character, in columns
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: card.report.changes.length > 0
            spacing: 0

            TextMetrics {
                id: number

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
                model: card.report.changes

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
                            colorblind: card.popup.colorblind
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
                        colorblind: card.popup.colorblind
                    }
                }
            }
        }
    }
}
