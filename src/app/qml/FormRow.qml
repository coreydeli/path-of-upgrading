// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// A card's row with its text in the middle and what's put in it on the
// right. It lays them out itself, as a FormTextDelegate's trailing item
// stays hidden for good once its page or card has been hidden.
FormCard.AbstractFormDelegate {
    id: row

    property string description
    // a smaller line under the description
    property string detail
    // Set for a row of a table, with the description in a column of its own
    // and the controls in one, as wide as these. Rows with the same widths
    // line their columns up, and a TableHeader over them takes the same.
    property real descriptionWidth: 0
    property real controlsWidth: 0
    // on the left, like a radio button
    property alias leading: leadingRow.data
    default property alias controls: controlsRow.data

    focusPolicy: Qt.NoFocus
    background: null

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            id: leadingRow
            visible: children.length > 0
            spacing: 0
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: row.text
                elide: Text.ElideRight
                // the row has it
                Accessible.ignored: true
            }

            QQC2.Label {
                objectName: "descriptionText"
                Layout.fillWidth: true
                visible: row.description !== "" && row.descriptionWidth <= 0
                text: row.description
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
            }

            QQC2.Label {
                objectName: "detailText"
                Layout.fillWidth: true
                visible: row.detail !== ""
                text: row.detail
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
            }
        }

        QQC2.Label {
            objectName: "descriptionCell"
            Layout.preferredWidth: row.descriptionWidth
            visible: row.descriptionWidth > 0
            text: row.description
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.Wrap
        }

        RowLayout {
            id: controlsRow
            Layout.preferredWidth: row.controlsWidth > 0 ? row.controlsWidth : -1
            visible: children.length > 0
            spacing: Kirigami.Units.smallSpacing
        }
    }
}
