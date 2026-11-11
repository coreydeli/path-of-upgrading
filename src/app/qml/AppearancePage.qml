// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// The colors of the app, how much is drawn besides them, and its fonts.
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

    readonly property string headingName: {
        const font = appearance.headingFont;
        if (font === "")
            return appearance.headingFamily;
        return font === "text" ? qsTr("The same as the text") : font;
    }
    readonly property string textName: appearance.textFont === "" ? appearance.textFamily : appearance.textFont

    title: qsTr("Appearance")
    description: qsTr("The colors, the extras that are drawn and the fonts.")

    SectionHeader {
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

    SectionHeader {
        title: qsTr("Fonts")
    }

    FormCard.FormCard {
        FormCard.FormButtonDelegate {
            objectName: "headingFontButton"
            text: qsTr("Heading font")
            description: appearancePage.headingName
            onClicked: headingFontPicker.open()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            objectName: "textFontButton"
            text: qsTr("Text font")
            description: appearancePage.textName
            onClicked: textFontPicker.open()
        }

        FormCard.FormDelegateSeparator {}

        SwitchRow {
            objectName: "desktopSizeSwitch"
            text: qsTr("Use your desktop's text size")
            checked: appearancePage.appearance.textSize === 0
            onToggled: appearancePage.appearance.textSize = checked ? 0 : Math.round(appearancePage.appearance.desktopTextSize)
        }

        FormCard.FormDelegateSeparator {
            visible: textSize.visible
        }

        NumberField {
            id: textSize

            objectName: "textSize"
            visible: appearancePage.appearance.textSize > 0
            label: qsTr("Text size")
            description: qsTr("In points. The controls and the spacing grow with it.")
            value: appearancePage.appearance.textSize
            minimum: 6
            maximum: 32
            step: 0.5
            decimals: 1
            onEdited: value => appearancePage.appearance.textSize = value
        }

        FormCard.FormDelegateSeparator {
            visible: fontsError.visible
        }

        FormCard.FormTextDelegate {
            id: fontsError

            objectName: "fontsError"
            visible: text !== ""
            text: appearancePage.appearance.fontsError
            textItem.wrapMode: Text.Wrap
        }
    }

    // what the choices look like together, as it changes
    FormCard.FormCard {
        FormCard.AbstractFormDelegate {
            background: null
            focusPolicy: Qt.NoFocus

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    objectName: "headingSample"
                    Layout.fillWidth: true
                    text: qsTr("Grim Crown")
                    font: Style.titleFont(Kirigami.Theme.defaultFont)
                    elide: Text.ElideRight
                }

                QQC2.Label {
                    objectName: "textSample"
                    Layout.fillWidth: true
                    text: qsTr("+92 to maximum Life")
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    FontPickerDialog {
        id: headingFontPicker

        objectName: "headingFontPicker"
        title: qsTr("Heading font")
        current: appearancePage.appearance.headingFont
        onAboutToShow: families = appearancePage.appearance.families()
        specials: [
            {
                value: "",
                text: qsTr("Big Shoulders Display"),
                family: "Big Shoulders Display"
            },
            {
                value: "text",
                text: qsTr("The same as the text")
            }
        ]
        onPicked: value => appearancePage.appearance.headingFont = value
    }

    FontPickerDialog {
        id: textFontPicker

        objectName: "textFontPicker"
        title: qsTr("Text font")
        current: appearancePage.appearance.textFont
        onAboutToShow: families = appearancePage.appearance.families()
        specials: [
            {
                value: "",
                text: qsTr("Spectral"),
                family: "Spectral"
            }
        ]
        onPicked: value => appearancePage.appearance.textFont = value
    }
}
