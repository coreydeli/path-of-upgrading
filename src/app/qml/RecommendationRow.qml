// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// A slot worth upgrading: what's worn in it, what to look for in a better
// one, and a trade search for it. Wide, those sit side by side. Narrow, what
// to look for goes under the other two. It lays them out itself, for the
// same reason SlotRow does.
FormCard.AbstractFormDelegate {
    id: row

    required property string slot
    required property string name
    required property string base
    required property string score
    required property string grade
    // "Life 55 or more, Fire res 24 or more"
    required property string lookFor
    // what its trade search is up to
    required property tradeReport trade
    // whether trade searches are on
    required property bool tradeAvailable
    // the first of its card, which has no line above it
    required property bool first

    readonly property bool wide: width >= Kirigami.Units.gridUnit * 38

    signal findAsked
    signal showResultsAsked

    objectName: "recommendation:" + slot
    text: slot
    focusPolicy: Qt.NoFocus
    background: null
    topPadding: Kirigami.Units.largeSpacing
    bottomPadding: Kirigami.Units.largeSpacing
    Accessible.name: slot
    Accessible.description: name !== "" ? qsTr("%1, look for %2").arg(name).arg(lookFor) : qsTr("Nothing saved, look for %1").arg(lookFor)

    Kirigami.Separator {
        visible: !row.first
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            leftMargin: Kirigami.Units.largeSpacing
            rightMargin: Kirigami.Units.largeSpacing
        }
    }

    contentItem: GridLayout {
        columns: row.wide ? 3 : 2
        columnSpacing: Kirigami.Units.gridUnit
        rowSpacing: Kirigami.Units.smallSpacing

        // the slot, then the item in it
        ColumnLayout {
            Layout.row: 0
            Layout.column: 0
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: !row.wide
            Layout.preferredWidth: row.wide ? Kirigami.Units.gridUnit * 14 : -1
            Layout.maximumWidth: row.wide ? Kirigami.Units.gridUnit * 14 : -1
            spacing: 0

            QQC2.Label {
                objectName: "slotName"
                Layout.fillWidth: true
                text: row.slot
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Accessible.ignored: true
            }

            QQC2.Label {
                objectName: "itemName"
                Layout.fillWidth: true
                text: row.name !== "" ? row.name : qsTr("Nothing saved")
                color: row.name !== "" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }

            QQC2.Label {
                objectName: "itemBase"
                Layout.fillWidth: true
                visible: text !== ""
                text: row.name !== "" ? row.base : ""
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                wrapMode: Text.Wrap
                Accessible.ignored: true
            }
        }

        // what to look for, and how the search is going
        ColumnLayout {
            Layout.row: row.wide ? 0 : 1
            Layout.column: row.wide ? 1 : 0
            Layout.columnSpan: row.wide ? 1 : 2
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                visible: row.lookFor !== ""
                spacing: Kirigami.Units.largeSpacing

                QQC2.Label {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                    text: qsTr("Look for")
                    color: Kirigami.Theme.disabledTextColor
                }

                QQC2.Label {
                    objectName: "lookFor"
                    Layout.fillWidth: true
                    text: row.lookFor
                    wrapMode: Text.Wrap
                }
            }

            RowLayout {
                objectName: "tradeStatus"
                Layout.fillWidth: true
                visible: row.trade.asked
                spacing: Kirigami.Units.smallSpacing

                QQC2.BusyIndicator {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    Layout.alignment: Qt.AlignTop
                    visible: row.trade.busy
                    running: visible
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    ToneLabel {
                        objectName: "tradeStatusText"
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: row.trade.status
                        tone: row.trade.failed ? Tone.Bad : Tone.Plain
                    }

                    QQC2.Label {
                        objectName: "tradeNote"
                        Layout.fillWidth: true
                        visible: text !== ""
                        wrapMode: Text.Wrap
                        opacity: 0.7
                        font: Kirigami.Theme.smallFont
                        text: row.trade.note
                    }
                }
            }
        }

        // the gear score and the search
        ColumnLayout {
            Layout.row: 0
            Layout.column: row.wide ? 2 : 1
            Layout.alignment: Qt.AlignTop | Qt.AlignRight
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                objectName: "gearScore"
                Layout.alignment: Qt.AlignRight
                visible: row.score !== ""
                text: qsTr("Gear score %1 %2").arg(row.score).arg(row.grade)
                font.weight: Font.DemiBold
            }

            QQC2.Button {
                objectName: "findButton"
                Layout.alignment: Qt.AlignRight
                visible: row.tradeAvailable && row.name !== ""
                enabled: !row.trade.busy
                text: qsTr("Find an Upgrade")
                icon.name: "edit-find"
                onClicked: row.findAsked()
            }

            QQC2.Button {
                objectName: "showResultsButton"
                Layout.alignment: Qt.AlignRight
                visible: row.trade.listed
                text: qsTr("Show Results")
                icon.name: "view-list-details"
                onClicked: row.showResultsAsked()
            }
        }
    }
}
