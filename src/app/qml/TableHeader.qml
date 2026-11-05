// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The titles over the columns of a table of FormRows. Give it the widths the
// rows have, and the columns line up. A screen reader gets what each row says
// on its own, so this is left out for it.
FormCard.AbstractFormDelegate {
    id: header

    property string first
    property string second
    property string third
    property real descriptionWidth: Kirigami.Units.gridUnit * 20
    property real controlsWidth: Kirigami.Units.gridUnit * 12

    focusPolicy: Qt.NoFocus
    background: null
    Accessible.ignored: true

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            objectName: "firstLabel"
            Layout.fillWidth: true
            text: header.first
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            elide: Text.ElideRight
        }

        QQC2.Label {
            objectName: "secondLabel"
            Layout.preferredWidth: header.descriptionWidth
            text: header.second
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            elide: Text.ElideRight
        }

        QQC2.Label {
            objectName: "thirdLabel"
            Layout.preferredWidth: header.controlsWidth
            text: header.third
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            elide: Text.ElideRight
        }
    }
}
