import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// A form row for a number, see NumberBox.
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

        NumberBox {
            objectName: "spinBox"
            number: row.value
            minimum: row.minimum
            maximum: row.maximum
            step: row.step
            decimals: row.decimals
            noneText: row.noneText
            onEdited: number => row.edited(number)
            Accessible.name: row.label
        }
    }
}
