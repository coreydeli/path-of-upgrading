// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kirigami as Kirigami

// The popup over the game, see ItemPopup.
Window {
    id: window

    required property ItemPopup popup

    width: Math.ceil(card.implicitWidth * popup.scale)
    height: Math.ceil(card.implicitHeight * popup.scale)
    color: "transparent"
    title: qsTr("Path of Upgrading")

    // It fades in and rises a little. The duration is 0 when the desktop has
    // animations off, and it isn't done on closing.
    onVisibleChanged: {
        if (visible && !Style.plain)
            appear.restart();
    }

    PopupCard {
        id: card

        popup: window.popup
        width: implicitWidth
        height: implicitHeight
        scale: window.popup.scale
        transformOrigin: Item.TopLeft
    }

    ParallelAnimation {
        id: appear

        NumberAnimation {
            target: card
            property: "opacity"
            from: 0
            to: 1
            duration: Kirigami.Units.shortDuration
            easing.type: Easing.OutQuad
        }

        NumberAnimation {
            target: card
            property: "y"
            from: 6
            to: 0
            duration: Kirigami.Units.shortDuration
            easing.type: Easing.OutQuad
        }
    }
}
