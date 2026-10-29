pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Finds a stat by the words in its name. Up and Down go through what it
// found, and Enter picks one.
FormCard.FormCardDialog {
    id: dialog

    required property StatPicker picker
    // The stats picked already, which show but can't be again.
    property list<string> taken
    // What those say in place of their note.
    property string takenNote: qsTr("You have it already")

    signal picked(string key)

    // A stat it found.
    component StatRow: QQC2.ItemDelegate {
        id: statRow

        required property string key
        required property string name
        required property string note
        required property bool taken

        objectName: "stat:" + key
        width: ListView.view.width
        enabled: !taken
        highlighted: ListView.isCurrentItem
        text: name
        Accessible.description: noteText.text
        onClicked: dialog.pick(key)

        contentItem: ColumnLayout {
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: statRow.name
                elide: Text.ElideRight
            }

            QQC2.Label {
                id: noteText

                objectName: "statNote"
                Layout.fillWidth: true
                visible: text !== ""
                text: statRow.taken ? dialog.takenNote : statRow.note
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
        }
    }

    function pick(key: string) {
        picked(key);
        close();
    }

    function pickCurrent() {
        const row = statList.currentItem as StatRow;
        if (row && !row.taken)
            pick(row.key);
    }

    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 30)
    title: qsTr("Add a Stat")
    standardButtons: QQC2.Dialog.Cancel

    onAboutToShow: {
        searchField.clear();
        picker.taken = Qt.binding(() => dialog.taken);
        picker.refresh();
    }
    onOpened: searchField.forceActiveFocus()

    Kirigami.SearchField {
        id: searchField

        objectName: "statSearch"
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing
        placeholderText: qsTr("Search the stats")
        autoAccept: false
        onTextChanged: dialog.picker.query = text
        Keys.onReturnPressed: dialog.pickCurrent()
        Keys.onEnterPressed: dialog.pickCurrent()
        Keys.onUpPressed: statList.decrementCurrentIndex()
        Keys.onDownPressed: statList.incrementCurrentIndex()
    }

    Kirigami.InlineMessage {
        objectName: "pickerMessage"
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        visible: text !== ""
        text: dialog.picker.message
    }

    FormCard.FormDelegateSeparator {}

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(Kirigami.Units.gridUnit * 16, dialog.parent.height * 0.5)
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        ListView {
            id: statList

            objectName: "statList"
            clip: true
            model: dialog.picker
            highlightMoveDuration: 0
            delegate: StatRow {}

            // Back to the first of what it found, as the row it was on may be
            // gone.
            Connections {
                target: dialog.picker

                function onModelReset() {
                    statList.currentIndex = 0;
                }
            }

            Kirigami.PlaceholderMessage {
                objectName: "noStats"
                // rather than the list's content, which scrolls
                parent: statList
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: statList.count === 0
                text: qsTr("No stat has those words.")
            }
        }
    }
}
