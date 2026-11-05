// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The history of the items you checked, to have the popup show again.
ColumnsPage {
    id: checkedPage

    required property CheckedItems checkedItems
    required property OverlayOptions options
    readonly property string key: options.checkKey.toString()

    // The item shown beside the list on a wide window: a row of the list, and
    // what CheckedItems has to say about it.
    property int picked: 0
    property var details: ({})

    function pick(row: int): void {
        picked = row;
        details = checkedItems.details(row);
    }

    title: qsTr("Item History")
    description: qsTr("The items you checked, to look at again.")
    maxColumns: wide ? 2 : 3

    // checking an item again moves it to the top
    Connections {
        target: checkedPage.checkedItems

        function onModelReset() {
            checkedPage.pick(0);
        }
    }

    Component.onCompleted: pick(0)

    // An item you checked, and what it is.
    component ItemRow: FormRow {
        id: itemRow

        required property int index
        required property string name
        required property string base
        required property string itemClass
        required property string rarity
        required property int itemLevel
        required property int requiredLevel

        objectName: "checked:" + index
        text: name
        // picked to show beside the list, as a row of a list is
        focusPolicy: checkedPage.wide ? Qt.StrongFocus : Qt.NoFocus
        onClicked: checkedPage.pick(index)
        Accessible.role: checkedPage.wide ? Accessible.ListItem : Accessible.Button
        Accessible.selectable: checkedPage.wide
        Accessible.selected: checkedPage.wide && index === checkedPage.picked
        background: Rectangle {
            visible: checkedPage.wide
            color: itemRow.index === checkedPage.picked ? Style.tint(Kirigami.Theme.backgroundColor, Style.gold(Kirigami.Theme), 0.18)
                                                         : itemRow.hovered || itemRow.visualFocus ? Style.tint(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.08)
                                                                                                  : "transparent"
        }
        description: {
            const parts = [];
            for (const part of [rarity, base, itemClass]) {
                if (part !== "")
                    parts.push(part);
            }
            if (itemLevel > 0)
                parts.push(qsTr("item level %1").arg(itemLevel));
            else if (requiredLevel > 0)
                parts.push(qsTr("needs level %1").arg(requiredLevel));
            return parts.join(", ");
        }

        QQC2.ToolButton {
            objectName: "checkAgainButton"
            text: qsTr("Check Again")
            icon.name: "view-refresh"
            display: QQC2.AbstractButton.IconOnly
            Accessible.name: qsTr("Check %1 Again").arg(itemRow.name)
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: checkedPage.checkedItems.checkAgain(itemRow.index)
        }
    }

    SectionHeader {
        objectName: "checkedHeader"
        column: checkedPage.wide ? 0 : -1
        title: qsTr("Newest First")
    }

    SectionText {
        objectName: "historyIntro"
        text: qsTr("The items you check in the game are kept here, the newest first, up to the last %1: those you check with the hotkey, and gear you copy yourself while \"Also check gear you copy yourself\" is on. When the list is full, the oldest drops off.").arg(checkedPage.checkedItems.limit)
    }

    FormCard.FormCard {
        id: itemList

        Repeater {
            id: itemRows
            model: checkedPage.checkedItems

            delegate: ItemRow {}
        }

        FormCard.FormTextDelegate {
            objectName: "noItemsText"
            visible: itemRows.count === 0
            text: qsTr("You haven't checked any items yet.")
            description: checkedPage.key !== "" ? qsTr("Hover over an item in the game and press %1.").arg(checkedPage.key) : qsTr("Hover over an item in the game and press the hotkey for checking one. You can set it on the Settings page.")
            textItem.wrapMode: Text.Wrap
            descriptionItem.wrapMode: Text.Wrap
        }

        FormCard.FormDelegateSeparator {
            objectName: "forgetSeparator"
            visible: itemRows.count > 0
        }

        FormCard.FormButtonDelegate {
            objectName: "forgetButton"
            visible: itemRows.count > 0
            text: qsTr("Forget Them")
            icon.name: "edit-clear-history"
            onClicked: forgetDialog.open()
        }
    }

    SectionText {
        objectName: "checkedHelp"
        visible: itemRows.count > 0
        text: qsTr("Check Again shows an item in the popup again, judged for the character you play now. Forgetting the items doesn't forget their stats, which the stat picker still offers.")
    }

    SectionHeader {
        objectName: "detailHeader"
        visible: checkedPage.wide && itemRows.count > 0
        column: checkedPage.wide ? 1 : -1
        title: qsTr("The Item")
    }

    ItemPanel {
        objectName: "itemPanel"
        Layout.fillWidth: true
        Layout.margins: checkedPage.gutter
        visible: checkedPage.wide && itemRows.count > 0
        details: checkedPage.details
        scrolling: checkedPage.flickable
        room: itemList.height
        onCheckAgainAsked: checkedPage.checkedItems.checkAgain(checkedPage.picked)
    }

    FormCard.FormCardDialog {
        id: forgetDialog
        objectName: "forgetDialog"

        title: qsTr("Forget the Items?")
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        onAccepted: checkedPage.checkedItems.forget()

        Component.onCompleted: {
            const forget = standardButton(QQC2.Dialog.Ok);
            forget.text = qsTr("Forget Them");
            forget.icon.name = "edit-clear-history";
        }

        FormCard.FormTextDelegate {
            text: qsTr("The list will be empty. You can check an item again in the game.")
            textItem.wrapMode: Text.Wrap
        }
    }
}
