import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// A card's row with its text on the left and what's put in it on the right.
// It lays them out itself, as a FormTextDelegate's trailing item stays
// hidden for good once its card has been hidden.
FormCard.AbstractFormDelegate {
    id: row

    property string description
    default property alias controls: controlsRow.data

    focusPolicy: Qt.NoFocus
    background: null

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: row.text
                elide: Text.ElideRight
                // the row has it
                Accessible.ignored: true
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: row.description !== ""
                text: row.description
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            id: controlsRow
            spacing: Kirigami.Units.smallSpacing
        }
    }
}
