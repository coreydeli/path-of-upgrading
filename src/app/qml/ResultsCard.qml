// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What the results window shows: the listings of the last trade search.
Rectangle {
    id: card

    required property ResultsPanel panel
    readonly property TradeResults results: panel.results
    // "12 listed in Runes of Aldur"
    readonly property string found: {
        if (!results.searched)
            return "";
        if (results.total === 0)
            return qsTr("None listed in %1 right now").arg(results.league);
        return qsTr("%1 listed in %2").arg(Number(results.total).toLocaleString(Qt.locale("en_US"), "f", 0))
                                      .arg(results.league);
    }
    // what it waits for while it asks the site
    readonly property string progress: {
        if (results.waiting === 0)
            return qsTr("Asking the trade site for the listings");
        return results.waiting === 1 ? qsTr("Waiting a second for the trade site")
                                     : qsTr("Waiting %1 seconds for the trade site").arg(results.waiting);
    }

    // dark whatever the desktop's colours, like the popup
    Kirigami.Theme.colorSet: Kirigami.Theme.Complementary
    Kirigami.Theme.inherit: false

    implicitWidth: Kirigami.Units.gridUnit * 26
    implicitHeight: content.implicitHeight + 2 * Kirigami.Units.largeSpacing
    color: Kirigami.Theme.backgroundColor
    radius: Kirigami.Units.cornerRadius
    border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor,
                                                          Kirigami.Theme.textColor, 0.2)

    ColumnLayout {
        id: content

        anchors {
            fill: parent
            margins: Kirigami.Units.largeSpacing
        }
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 3
                    wrapMode: Text.Wrap
                    text: card.results.searched ? card.results.title : qsTr("Trade Results")
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    visible: text !== ""
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    text: card.found
                }
            }

            QQC2.ToolButton {
                Layout.alignment: Qt.AlignTop
                icon.name: "window-close"
                text: qsTr("Close")
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: card.panel.close()
            }
        }

        // before there's a search to show
        QQC2.Label {
            Layout.fillWidth: true
            visible: !card.results.searched
            wrapMode: Text.Wrap
            opacity: 0.7
            text: card.panel.unavailable !== ""
                  ? card.panel.unavailable
                  : qsTr("Check an item and search the trade site from the popup first.")
        }

        ListView {
            id: listings

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: count > 0
            implicitHeight: contentHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: card.results
            delegate: ResultRow {
                colorblind: card.panel.colorblind
                // the bar is always the same width, so the text doesn't move when it comes and goes
                clearance: scrollBar.implicitWidth + Kirigami.Units.smallSpacing
            }

            QQC2.ScrollBar.vertical: QQC2.ScrollBar {
                id: scrollBar
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: card.results.busy
            spacing: Kirigami.Units.smallSpacing

            QQC2.BusyIndicator {
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                running: parent.visible
            }

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.7
                text: card.progress
            }
        }

        ToneLabel {
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            text: card.results.error
            tone: Tone.Bad
            colorblind: card.panel.colorblind
        }

        // the site lists the first hundred and no more
        QQC2.Label {
            Layout.fillWidth: true
            visible: !card.results.more && card.results.available < card.results.total
            wrapMode: Text.Wrap
            opacity: 0.7
            font: Kirigami.Theme.smallFont
            text: qsTr("The trade site lists only the first %1. Open it to see the rest.")
                  .arg(card.results.available)
        }

        RowLayout {
            Layout.fillWidth: true
            visible: card.results.more || card.results.url.toString() !== ""
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                visible: card.results.more
                enabled: !card.results.busy
                text: listings.count === 0 ? qsTr("Try Again") : qsTr("Load More")
                icon.name: "view-refresh"
                onClicked: card.results.loadMore()
            }

            QQC2.Button {
                visible: card.results.url.toString() !== ""
                text: qsTr("Open on the Trade Site")
                icon.name: "internet-web-browser"
                // in your browser, where you can sign in to buy
                onClicked: Qt.openUrlExternally(card.results.url)
            }
        }
    }
}
