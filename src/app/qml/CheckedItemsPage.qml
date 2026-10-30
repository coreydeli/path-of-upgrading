// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The items you checked lately, to have the popup show again.
FormCard.FormCardPage {
    id: checkedPage

    required property CheckedItems checkedItems
    required property OverlayOptions options
    readonly property string key: options.checkKey.toString()

    title: qsTr("Checked Items")

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

    FormCard.FormHeader {
        objectName: "checkedHeader"
        title: qsTr("Newest First")
    }

    FormCard.FormCard {
        Repeater {
            id: itemRows
            model: checkedPage.checkedItems

            delegate: ItemRow {}
        }

        FormCard.FormTextDelegate {
            objectName: "noItemsText"
            visible: itemRows.count === 0
            text: qsTr("You haven't checked any items yet.")
            description: checkedPage.key !== "" ? qsTr("Hover over an item in the game and press %1.").arg(checkedPage.key) : qsTr("Hover over an item in the game and press the hotkey for checking one. You can set it on the Popup and Hotkeys page.")
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

    FormCard.FormSectionText {
        objectName: "checkedHelp"
        visible: itemRows.count > 0
        text: qsTr("Check Again shows an item in the popup again, judged for the character you play now. Forgetting the items doesn't forget their stats, which the stat picker still offers.")
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
