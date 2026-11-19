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
    readonly property var designChoices: appearance.designs.map(design => ({
                value: design.id,
                text: design.name
            }))
    // what the look picked says of itself
    readonly property string designNote: appearance.designs.find(design => design.id === appearance.design)?.note ?? ""
    readonly property var brightnessChoices: [
        {
            value: "desktop",
            text: qsTr("Your desktop's")
        },
        {
            value: "light",
            text: qsTr("Light")
        },
        {
            value: "dark",
            text: qsTr("Dark")
        }
    ]
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
        return font === "text" ? qsTr("Same as the text font") : font;
    }
    readonly property string textName: appearance.textFont === "" ? appearance.textFamily : appearance.textFont

    title: qsTr("Appearance")
    description: qsTr("Pick a look, colors and fonts.")

    SectionHeader {
        title: qsTr("Theme")
    }

    Sheet {
        FormCard.FormComboBoxDelegate {
            objectName: "designBox"
            text: qsTr("Look")
            description: qsTr("%1. It changes right away.").arg(appearancePage.designNote)
            textRole: "text"
            valueRole: "value"
            model: appearancePage.designChoices
            currentIndex: appearancePage.designChoices.findIndex(choice => choice.value === appearancePage.appearance.design)
            onActivated: appearancePage.appearance.design = currentValue
        }

        RowRule {}

        FormCard.FormComboBoxDelegate {
            objectName: "colorsBox"
            text: qsTr("Colors")
            description: appearancePage.appearance.hasLight ? qsTr("The look's own colors, or the ones your desktop uses.") : qsTr("The app's own dark colors, or the ones your desktop uses.")
            textRole: "text"
            valueRole: "value"
            model: appearancePage.colorChoices
            currentIndex: appearancePage.colorChoices.findIndex(choice => choice.value === appearancePage.appearance.colors)
            onActivated: appearancePage.appearance.colors = currentValue
        }

        RowRule {
            visible: brightnessBox.visible
        }

        FormCard.FormComboBoxDelegate {
            id: brightnessBox

            objectName: "brightnessBox"
            visible: appearancePage.appearance.hasLight && appearancePage.appearance.colors === "app"
            text: qsTr("Light or dark")
            description: qsTr("The look's colors can be light or dark, or follow your desktop.")
            textRole: "text"
            valueRole: "value"
            model: appearancePage.brightnessChoices
            currentIndex: appearancePage.brightnessChoices.findIndex(choice => choice.value === appearancePage.appearance.brightness)
            onActivated: appearancePage.appearance.brightness = currentValue
        }

        RowRule {
            visible: colorsError.visible
        }

        FormCard.FormTextDelegate {
            id: colorsError
            objectName: "colorsError"
            visible: text !== ""
            text: appearancePage.appearance.colorsError
            textItem.wrapMode: Text.Wrap
        }

        RowRule {}

        FormCard.FormComboBoxDelegate {
            objectName: "decorationsBox"
            text: qsTr("Decorations")
            description: appearancePage.appearance.design === "dossier" ? qsTr("Plain gives square corners and no animation.") : qsTr("Plain leaves out the animation.")
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

    Sheet {
        FormCard.FormButtonDelegate {
            objectName: "headingFontButton"
            text: qsTr("Heading font")
            description: appearancePage.headingName
            onClicked: headingFontPicker.open()
        }

        RowRule {}

        FormCard.FormButtonDelegate {
            objectName: "textFontButton"
            text: qsTr("Text font")
            description: appearancePage.textName
            onClicked: textFontPicker.open()
        }

        RowRule {}

        SwitchRow {
            objectName: "desktopSizeSwitch"
            text: qsTr("Use your desktop's text size")
            checked: appearancePage.appearance.textSize === 0
            onToggled: appearancePage.appearance.textSize = checked ? 0 : Math.round(appearancePage.appearance.desktopTextSize)
        }

        RowRule {
            visible: textSize.visible
        }

        NumberField {
            id: textSize

            objectName: "textSize"
            visible: appearancePage.appearance.textSize > 0
            label: qsTr("Text size")
            description: qsTr("Measured in points. The controls and the spacing grow with it.")
            value: appearancePage.appearance.textSize
            minimum: 6
            maximum: 32
            step: 0.5
            decimals: 1
            onEdited: value => appearancePage.appearance.textSize = value
        }

        RowRule {
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
    Sheet {
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
            appearancePage.appearance.designHeadingFont === "" ? {
                value: "",
                text: qsTr("Your desktop's")
            } : {
                value: "",
                text: appearancePage.appearance.designHeadingFont,
                family: appearancePage.appearance.designHeadingFont
            },
            {
                value: "text",
                text: qsTr("Same as the text font")
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
            appearancePage.appearance.designTextFont === "" ? {
                value: "",
                text: qsTr("Your desktop's")
            } : {
                value: "",
                text: appearancePage.appearance.designTextFont,
                family: appearancePage.appearance.designTextFont
            }
        ]
        onPicked: value => appearancePage.appearance.textFont = value
    }
}
