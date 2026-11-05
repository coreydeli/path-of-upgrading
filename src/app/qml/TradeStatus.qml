// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// How a trade search is going: a spinner while it waits, what it found or why
// it didn't, and a note on how it had to search. It's there only once a search
// has been asked for.
RowLayout {
    id: status

    // what the search is up to
    property tradeReport trade

    objectName: "tradeStatus"
    visible: trade.asked
    spacing: Kirigami.Units.smallSpacing

    QQC2.BusyIndicator {
        Layout.preferredWidth: Kirigami.Units.iconSizes.small
        Layout.preferredHeight: Kirigami.Units.iconSizes.small
        Layout.alignment: Qt.AlignTop
        visible: status.trade.busy
        running: visible
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0

        ToneLabel {
            objectName: "tradeStatusText"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: status.trade.status
            tone: status.trade.failed ? Tone.Bad : Tone.Plain
        }

        QQC2.Label {
            objectName: "tradeNote"
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            opacity: 0.7
            font: Kirigami.Theme.smallFont
            text: status.trade.note
        }
    }
}
