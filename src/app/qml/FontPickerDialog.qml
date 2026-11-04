// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Finds an installed font by the words in its name, each drawn in its own
// font. The choices that aren't a font, like "Your desktop's", come first.
FormCard.FormCardDialog {
    id: dialog

    // what it says for each: value, text and the family to draw it in, if any
    property var specials: []
    // the value chosen now
    property string current
    // the fonts to choose from
    property var families: []

    readonly property var entries: {
        const words = searchField.text.trim().toLowerCase();
        const rows = [];
        for (const special of specials) {
            if (words === "" || special.text.toLowerCase().includes(words))
                rows.push({
                    value: special.value,
                    text: special.text,
                    family: special.family ?? ""
                });
        }
        for (const family of families) {
            if (words === "" || family.toLowerCase().includes(words))
                rows.push({
                    value: family,
                    text: family,
                    family: family
                });
        }
        return rows;
    }

    signal picked(string value)

    function pick(value: string) {
        picked(value);
        close();
    }

    function pickCurrent() {
        const row = fontList.currentItem as FontRow;
        if (row)
            pick(row.value);
    }

    component FontRow: QQC2.ItemDelegate {
        id: fontRow

        required property string value
        required property string family
        required property bool chosen

        objectName: "font:" + value
        width: ListView.view.width
        highlighted: ListView.isCurrentItem
        font.family: family !== "" ? family : Kirigami.Theme.defaultFont.family
        onClicked: dialog.pick(value)

        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                Layout.fillWidth: true
                text: fontRow.text
                font: fontRow.font
                color: fontRow.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                elide: Text.ElideRight
            }

            Kirigami.Icon {
                visible: fontRow.chosen
                source: "checkmark"
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
            }
        }
    }

    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 26)
    standardButtons: QQC2.Dialog.Cancel

    onAboutToShow: searchField.clear()
    onOpened: searchField.forceActiveFocus()

    Kirigami.SearchField {
        id: searchField

        objectName: "fontSearch"
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing
        placeholderText: qsTr("Search the fonts")
        autoAccept: false
        Keys.onReturnPressed: dialog.pickCurrent()
        Keys.onEnterPressed: dialog.pickCurrent()
        Keys.onUpPressed: fontList.decrementCurrentIndex()
        Keys.onDownPressed: fontList.incrementCurrentIndex()
    }

    FormCard.FormDelegateSeparator {}

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(Kirigami.Units.gridUnit * 16, dialog.parent.height * 0.5)
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        ListView {
            id: fontList

            objectName: "fontList"
            clip: true
            model: dialog.entries
            highlightMoveDuration: 0
            delegate: FontRow {
                required property var modelData

                value: modelData.value
                text: modelData.text
                family: modelData.family
                chosen: modelData.value === dialog.current
            }

            // back to the first of what it found, as the row it was on may be
            // gone
            onModelChanged: currentIndex = 0

            Kirigami.PlaceholderMessage {
                objectName: "noFonts"
                // rather than the list's content, which scrolls
                parent: fontList
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: fontList.count === 0
                text: qsTr("No font has those words.")
            }
        }
    }
}
