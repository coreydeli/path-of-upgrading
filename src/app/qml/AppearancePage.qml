// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

// The colors of the app and how much is drawn besides them.
ColumnsPage {
    id: appearancePage

    required property Appearance appearance
    readonly property var colorChoices: [
        {
            value: "app",
            text: qsTr("Path of Upgrading")
        },
        {
            value: "desktop",
            text: qsTr("Your desktop's")
        }
    ]
    readonly property var decorationChoices: [
        {
            value: "full",
            text: qsTr("Full")
        },
        {
            value: "plain",
            text: qsTr("Plain")
        }
    ]

    title: qsTr("Appearance")

    FormCard.FormHeader {
        title: qsTr("Theme")
    }

    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            objectName: "colorsBox"
            text: qsTr("Colors")
            description: qsTr("The app's own dark colors, or the ones your desktop uses.")
            textRole: "text"
            valueRole: "value"
            model: appearancePage.colorChoices
            currentIndex: appearancePage.colorChoices.findIndex(choice => choice.value === appearancePage.appearance.colors)
            onActivated: appearancePage.appearance.colors = currentValue
        }

        FormCard.FormDelegateSeparator {
            visible: colorsError.visible
        }

        FormCard.FormTextDelegate {
            id: colorsError
            objectName: "colorsError"
            visible: text !== ""
            text: appearancePage.appearance.colorsError
            textItem.wrapMode: Text.Wrap
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormComboBoxDelegate {
            objectName: "decorationsBox"
            text: qsTr("Decorations")
            description: qsTr("Plain leaves out the corner pieces, the glow and the animation.")
            textRole: "text"
            valueRole: "value"
            model: appearancePage.decorationChoices
            currentIndex: appearancePage.decorationChoices.findIndex(choice => choice.value === appearancePage.appearance.decorations)
            onActivated: appearancePage.appearance.decorations = currentValue
        }
    }
}
