// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// What's at the top of the sidebar: the app's name, the character you play
// with its build, and its overall gear score.
Item {
    id: header

    required property App app
    readonly property string character: app.gearList.character
    readonly property string build: app.characterList.currentBuild
    readonly property string score: app.gearList.overall
    readonly property string grade: app.gearList.overallGrade

    implicitHeight: content.implicitHeight + 6 * Kirigami.Units.largeSpacing
    implicitWidth: Kirigami.Units.gridUnit * 14

    ColumnLayout {
        id: content

        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            margins: Kirigami.Units.largeSpacing * 2
        }
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            font: Style.titleFont(Kirigami.Theme.defaultFont, 1.15)
            // the name is a slab of paper
            color: Style.ink
            topPadding: 4
            bottomPadding: 4
            background: Rectangle {
                color: Style.paper
            }
            text: qsTr("Path of Upgrading")
            wrapMode: Text.WordWrap
        }

        ScoreGauge {
            Layout.alignment: Qt.AlignHCenter
            visible: header.score !== ""
            score: header.score
            grade: header.grade
            size: "medium"
        }

        QQC2.Label {
            objectName: "sidebarCharacter"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.bold: true
            text: header.character !== "" ? header.character : qsTr("No character yet")
        }

        QQC2.Label {
            objectName: "sidebarBuild"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            visible: text !== ""
            color: Kirigami.Theme.disabledTextColor
            text: header.build
        }
    }
}
