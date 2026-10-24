import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import org.kde.kquickcontrols as KQuickControls

// A form row for a hotkey's key. It shows the key it has, so when a key you
// press is refused, the one it had.
FormCard.AbstractFormDelegate {
    id: row

    property string label
    property string description
    // a QKeySequence
    property var key

    signal chosen(var key)

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

        KQuickControls.KeySequenceItem {
            id: recorder
            objectName: "recorder"
            keySequence: row.key
            // its replacement, patterns, is newer than Debian 13 has
            modifierlessAllowed: true // qmllint disable deprecated
            multiKeyShortcutsAllowed: false
            // the desktop says when another program has it
            checkForConflictsAgainst: KQuickControls.ShortcutType.None
            onKeySequenceModified: {
                row.chosen(keySequence);
                keySequence = Qt.binding(() => row.key);
            }
        }
    }
}
