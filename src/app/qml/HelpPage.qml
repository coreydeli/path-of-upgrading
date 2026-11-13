// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Where to read more: the wiki, and its page for each part of the app.
ColumnsPage {
    id: helpPage

    required property Help help

    title: qsTr("Help")
    description: qsTr("The wiki explains each part of the app. Its pages open in your browser.")

    SectionHeader {
        title: qsTr("The Wiki")
    }

    Sheet {
        FormCard.FormButtonDelegate {
            objectName: "wikiButton"
            text: qsTr("Open the Wiki")
            description: qsTr("All its pages, on GitHub.")
            icon.name: "help-contents"
            onClicked: helpPage.help.openWiki()
        }
    }

    SectionHeader {
        title: qsTr("Pages")
    }

    Sheet {
        Repeater {
            model: helpPage.help.topics

            delegate: FormCard.FormButtonDelegate {
                required property int index
                required property var modelData

                objectName: "topic:" + modelData.page
                text: modelData.title
                description: modelData.what
                icon.name: "help-contents"
                onClicked: helpPage.help.openTopic(index)
            }
        }
    }
}
