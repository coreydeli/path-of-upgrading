// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A few words of help under a card. It's FormSectionText without the limit
// on its width, so it lines up with the card it goes with.
QQC2.Label {
    color: Kirigami.Theme.disabledTextColor
    wrapMode: Text.Wrap
    Layout.fillWidth: true
    Layout.leftMargin: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
    Layout.rightMargin: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
    Layout.topMargin: Kirigami.Units.largeSpacing
    Layout.bottomMargin: Kirigami.Units.largeSpacing
}
