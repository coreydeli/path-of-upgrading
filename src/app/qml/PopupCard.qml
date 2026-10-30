pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What the popup shows: the item you checked and what it means for you.
Rectangle {
    id: card

    required property ItemPopup popup
    readonly property itemReport report: popup.report
    // compact, with what hardly matters waiting behind Show More
    readonly property bool folded: popup.compact && !popup.expanded
    // Under a verdict the notes are extras. Without one they say why it has
    // none.
    readonly property bool notesFold: report.verdict !== "" && report.notes.length > 0
    // the stats behind the verdict, which wait behind Show More
    readonly property bool hasDetails: report.statChanges.length > 0
    readonly property tradeReport trade: popup.trade

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

        // what's left to craft on it
        QQC2.Label {
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            opacity: 0.7
            text: card.report.openAffixes
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

        // how good it is for your build in that slot, next to what you wear
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: card.report.gearScore !== ""
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                font.bold: true
                text: card.report.gearScore + " " + card.report.gearGrade
            }

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.7
                text: {
                    const report = card.report;
                    const parts = [report.gearBuild !== "" ? qsTr("gear score for %1").arg(report.gearBuild)
                                                           : qsTr("gear score")];
                    if (report.yourGearScore !== "")
                        parts.push(qsTr("yours %1").arg(report.yourGearScore));
                    return parts.join(", ");
                }
            }
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

        ChangesTable {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: figures.length > 0
            figures: card.folded ? card.report.changes.filter(figure => !figure.minor) : card.report.changes
            colorblind: card.popup.colorblind
        }

        // every stat that changes, not only those that count
        ChangesTable {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: !card.folded && figures.length > 0
            title: qsTr("Stats that change")
            figures: card.report.statChanges
            colorblind: card.popup.colorblind
        }

        // where Path of Building is with it, while it hasn't judged it
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: card.report.pobStatus !== ""
            spacing: Kirigami.Units.smallSpacing

            QQC2.BusyIndicator {
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                visible: card.report.pobChecking
                running: visible
            }

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.7
                text: card.report.pobStatus
            }
        }

        Repeater {
            model: card.folded && card.notesFold ? [] : card.report.notes

            QQC2.Label {
                required property string modelData

                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                text: modelData
            }
        }

        QQC2.ToolButton {
            visible: card.popup.compact
                     && (card.report.changes.some(figure => figure.minor) || card.notesFold || card.hasDetails)
            text: card.popup.expanded ? qsTr("Show Less") : qsTr("Show More")
            icon.name: card.popup.expanded ? "arrow-up" : "arrow-down"
            onClicked: card.popup.expanded = !card.popup.expanded
        }

        // to save it as what you wear, which items are judged against
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: card.report.wearIn.length > 0
            spacing: Kirigami.Units.smallSpacing

            // in line with the first row when the buttons take two
            QQC2.Label {
                Layout.alignment: Qt.AlignTop
                Layout.preferredHeight: wearButtons.count > 0 ? wearButtons.itemAt(0).height : implicitHeight
                verticalAlignment: Text.AlignVCenter
                opacity: 0.7
                text: qsTr("Wear as")
            }

            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    id: wearButtons

                    model: card.report.wearIn

                    QQC2.ToolButton {
                        required property string modelData

                        text: modelData
                        onClicked: card.popup.wear(modelData)
                    }
                }
            }
        }

        ToneLabel {
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            text: card.popup.wearError
            tone: Tone.Bad
            colorblind: card.popup.colorblind
        }

        // searches on the trade site for gear like it
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            visible: card.popup.tradeAvailable && card.report.gear
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: [
                        {
                            kind: TradeFinder.Upgrades,
                            text: qsTr("Find Upgrades"),
                            icon: "go-up"
                        },
                        {
                            kind: TradeFinder.Similar,
                            text: qsTr("Same or Better"),
                            icon: "edit-find"
                        },
                        {
                            kind: TradeFinder.Weighted,
                            text: qsTr("By My Weights"),
                            icon: "view-statistics"
                        },
                        {
                            kind: TradeFinder.CraftBases,
                            text: qsTr("Bases to Craft On"),
                            icon: "tools-wizard"
                        }
                    ]

                    QQC2.ToolButton {
                        required property var modelData

                        text: modelData.text
                        icon.name: modelData.icon
                        enabled: !card.trade.busy
                        onClicked: card.popup.findOnTrade(modelData.kind)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: card.trade.asked
                spacing: Kirigami.Units.smallSpacing

                QQC2.BusyIndicator {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    Layout.alignment: Qt.AlignTop
                    visible: card.trade.busy
                    running: visible
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: text !== ""
                        wrapMode: Text.Wrap
                        font.bold: true
                        text: card.trade.title
                    }

                    ToneLabel {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: card.trade.status
                        tone: card.trade.failed ? Tone.Bad : Tone.Plain
                        colorblind: card.popup.colorblind
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: text !== ""
                        wrapMode: Text.Wrap
                        opacity: 0.7
                        font: Kirigami.Theme.smallFont
                        text: card.trade.note
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                visible: card.trade.url.toString() !== ""
                spacing: Kirigami.Units.smallSpacing

                QQC2.Button {
                    visible: card.trade.listed
                    text: qsTr("Show Results")
                    icon.name: "view-list-details"
                    onClicked: card.popup.showResults()
                }

                QQC2.Button {
                    text: qsTr("Open on the Trade Site")
                    icon.name: "internet-web-browser"
                    // in your browser, where you can sign in to buy
                    onClicked: Qt.openUrlExternally(card.trade.url)
                }
            }
        }
    }
}
