// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The character you play, beside the list of them: its build, its gear
// score and what that goes by, and what you can do with it.
Item {
    id: panel

    required property App app
    readonly property string character: app.gearList.character
    readonly property string build: app.characterList.currentBuild
    readonly property string score: app.gearList.overall
    readonly property string grade: app.gearList.overallGrade

    signal gearAsked
    signal renameAsked
    signal removeAsked

    implicitHeight: content.implicitHeight + 2 * Kirigami.Units.gridUnit

    Plate {
        anchors.fill: parent
        line: Style.frame(Kirigami.Theme)
        lineWidth: 1
    }

    ColumnLayout {
        id: content

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Kirigami.Units.gridUnit
        }
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                QQC2.Label {
                    objectName: "panelCharacter"
                    Layout.fillWidth: true
                    text: panel.character
                    color: Style.accent(Kirigami.Theme)
                    font: Style.titleFont(Kirigami.Theme.defaultFont, 1.4)
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.Heading
                }

                QQC2.Label {
                    objectName: "panelBuild"
                    Layout.fillWidth: true
                    text: panel.build !== "" ? panel.build : qsTr("No build imported")
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.Wrap
                }
            }

            ScoreGauge {
                objectName: "panelScore"
                Layout.alignment: Qt.AlignTop
                visible: panel.score !== ""
                score: panel.score
                grade: panel.grade
                size: "medium"
            }
        }

        RowRule {}

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                objectName: "panelProfile"
                Layout.fillWidth: true
                text: qsTr("Build profile: %1").arg(panel.app.gearScoring.profiles[panel.app.gearScoring.profile])
                font.bold: true
                wrapMode: Text.Wrap
            }

            QQC2.Label {
                objectName: "panelScoring"
                Layout.fillWidth: true
                text: panel.app.gearScoring.summary
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.Button {
                objectName: "panelGearButton"
                highlighted: true
                text: qsTr("View Gear")
                icon.name: "view-list-details"
                Accessible.name: qsTr("View Gear of %1").arg(panel.character)
                onClicked: panel.gearAsked()
            }

            QQC2.Button {
                objectName: "panelRenameButton"
                text: qsTr("Rename")
                icon.name: "edit-rename"
                Accessible.name: qsTr("Rename %1").arg(panel.character)
                onClicked: panel.renameAsked()
            }

            QQC2.Button {
                objectName: "panelRemoveButton"
                text: qsTr("Remove")
                icon.name: "edit-delete"
                Accessible.name: qsTr("Remove %1").arg(panel.character)
                onClicked: panel.removeAsked()
            }
        }
    }
}
