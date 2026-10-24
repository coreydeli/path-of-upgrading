import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// A form row for a number, with a decimal when it has decimals: 1, like 2.5
// Divine Orbs.
FormCard.AbstractFormDelegate {
    id: row

    property string label
    property string description
    property real value
    property real minimum: 0
    property real maximum: 100000
    property real step: 1
    property int decimals: 0
    // what 0 shows as, when it means there's none
    property string noneText
    // as the spin box counts in whole numbers
    readonly property real factor: Math.pow(10, decimals)

    signal edited(real value)

    background: null
    focusPolicy: Qt.NoFocus

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                Layout.fillWidth: true
                text: row.label
                wrapMode: Text.Wrap
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: text !== ""
                text: row.description
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
            }
        }

        QQC2.SpinBox {
            id: spinBox
            objectName: "spinBox"
            from: Math.round(row.minimum * row.factor)
            to: Math.round(row.maximum * row.factor)
            stepSize: Math.round(row.step * row.factor)
            editable: true
            value: Math.round(row.value * row.factor)
            textFromValue: (value, locale) => {
                if (value === 0 && row.noneText !== "")
                    return row.noneText;
                return Number(value / row.factor).toLocaleString(locale, "f", value % row.factor === 0 ? 0 : row.decimals);
            }
            valueFromText: (text, locale) => {
                try {
                    return Math.round(Number.fromLocaleString(locale, text) * row.factor);
                } catch (error) {
                    // the none text
                    return 0;
                }
            }
            validator: DoubleValidator {
                bottom: row.minimum
                top: row.maximum
                decimals: row.decimals
                notation: DoubleValidator.StandardNotation
                locale: spinBox.locale.name
            }
            onValueModified: row.edited(value / row.factor)
            Accessible.name: row.label
        }
    }
}
