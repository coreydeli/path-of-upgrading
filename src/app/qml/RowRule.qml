// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A thin rule across a card or a panel, between what it holds.
Rectangle {
    Layout.fillWidth: true
    implicitHeight: 1
    color: Style.inner(Kirigami.Theme)
    Accessible.ignored: true
}
