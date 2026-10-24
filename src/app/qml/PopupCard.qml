import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What the popup shows: the item you checked and what it means for you.
Rectangle {
    id: card

    required property ItemPopup popup
    readonly property itemReport report: popup.report

    // the game's colours for them
    function rarityColor(rarity: string): color {
        switch (rarity) {
        case "Magic":
            return "#8888ff";
        case "Rare":
            return "#ffff77";
        case "Unique":
            return "#af6025";
        default:
            return Kirigami.Theme.textColor;
        }
    }

    // dark whatever the desktop's colours, like the game
    Kirigami.Theme.colorSet: Kirigami.Theme.Complementary
    Kirigami.Theme.inherit: false

    implicitWidth: Kirigami.Units.gridUnit * 22
    implicitHeight: content.implicitHeight + 2 * Kirigami.Units.largeSpacing
    color: Kirigami.Theme.backgroundColor
    radius: Kirigami.Units.cornerRadius
    border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor,
                                                          Kirigami.Theme.textColor, 0.2)

    HoverHandler {
        onHoveredChanged: card.popup.hovered = hovered
    }

    ColumnLayout {
        id: content

        anchors {
            fill: parent
            margins: Kirigami.Units.largeSpacing
        }
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 3
                wrapMode: Text.Wrap
                text: card.popup.message || card.report.name
                color: card.popup.message ? Kirigami.Theme.textColor : card.rarityColor(card.report.rarity)
            }

            QQC2.ToolButton {
                Layout.alignment: Qt.AlignTop
                icon.name: "window-close"
                text: qsTr("Close")
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: card.popup.close()
            }
        }
    }
}
