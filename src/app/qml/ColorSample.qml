// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A row of the Color Key: a word in the color, and what the color means.
FormRow {
    id: row

    // the word shown, and the color it's drawn in
    property string sample
    property color tint: Kirigami.Theme.textColor
    // what it says to a screen reader, as the color itself varies
    property string meaning: text

    leading: QQC2.Label {
        objectName: "sample"
        Layout.preferredWidth: Kirigami.Units.gridUnit * 6
        text: row.sample
        color: row.tint
        font.bold: true
        Accessible.name: row.meaning
    }
}
