// SPDX-License-Identifier: MIT
import QtQuick

// The popup over the game, see ItemPopup.
Window {
    id: window

    required property ItemPopup popup

    width: Math.ceil(card.implicitWidth * popup.scale)
    height: Math.ceil(card.implicitHeight * popup.scale)
    color: "transparent"
    title: qsTr("Path of Upgrading")

    PopupCard {
        id: card

        popup: window.popup
        width: implicitWidth
        height: implicitHeight
        scale: window.popup.scale
        transformOrigin: Item.TopLeft
    }
}
