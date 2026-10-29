import QtQuick

// The window over the game with the trade results, see ResultsPanel.
Window {
    id: window

    required property ResultsPanel panel

    width: Math.ceil(card.width * panel.scale)
    height: Math.ceil(card.height * panel.scale)
    color: "transparent"
    title: qsTr("Trade Results")

    ResultsCard {
        id: card

        panel: window.panel
        width: implicitWidth
        // as tall as the listings, and no taller than the screen has room for
        height: Math.min(implicitHeight, window.panel.maxHeight / window.panel.scale)
        scale: window.panel.scale
        transformOrigin: Item.TopLeft
    }
}
