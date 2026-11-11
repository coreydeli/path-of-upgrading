// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// Which version this is, what it runs on, and who made what it uses.
ColumnsPage {
    id: aboutPage

    required property About about

    title: qsTr("About")
    description: qsTr("The version you have, where to find the project, and what it uses.")

    SectionHeader {
        title: qsTr("This Version")
    }

    Sheet {
        FormCard.FormTextDelegate {
            objectName: "versionText"
            text: qsTr("Version")
            description: qsTr("%1, a %2").arg(aboutPage.about.version).arg(aboutPage.about.edition.toLowerCase())
        }

        RowRule {}

        FormCard.FormTextDelegate {
            objectName: "licenseText"
            text: qsTr("License")
            description: aboutPage.about.license
        }

        RowRule {}

        FormCard.FormTextDelegate {
            objectName: "qtText"
            text: qsTr("Qt")
            description: aboutPage.about.qtVersion
        }

        RowRule {}

        FormCard.FormTextDelegate {
            objectName: "frameworksText"
            text: qsTr("KDE Frameworks")
            description: aboutPage.about.frameworksVersion
        }

        RowRule {}

        FormCard.FormTextDelegate {
            objectName: "pobText"
            text: qsTr("Path of Building")
            description: aboutPage.about.pobStatus
            descriptionItem.wrapMode: Text.Wrap
        }
    }

    SectionHeader {
        title: qsTr("Project")
    }

    Sheet {
        FormCard.FormButtonDelegate {
            objectName: "repositoryButton"
            text: qsTr("Source Code")
            description: qsTr("The repository on GitHub.")
            icon.name: "internet-web-browser"
            onClicked: aboutPage.about.openRepository()
        }

        RowRule {}

        FormCard.FormButtonDelegate {
            objectName: "releasesButton"
            text: qsTr("Releases")
            description: qsTr("Newer versions, and what changed in each.")
            icon.name: "internet-web-browser"
            onClicked: aboutPage.about.openReleases()
        }

        RowRule {}

        FormCard.FormButtonDelegate {
            objectName: "issuesButton"
            text: qsTr("Report a Problem")
            description: qsTr("The issue tracker, for bugs and ideas.")
            icon.name: "tools-report-bug"
            onClicked: aboutPage.about.openIssues()
        }
    }

    SectionHeader {
        title: qsTr("Credits")
    }

    Sheet {
        Repeater {
            model: aboutPage.about.credits

            delegate: FormCard.FormButtonDelegate {
                required property int index
                required property var modelData

                objectName: "credit:" + modelData.name
                text: modelData.name
                description: modelData.license !== "" ? qsTr("%1 (%2)").arg(modelData.what).arg(modelData.license) : modelData.what
                icon.name: "internet-web-browser"
                onClicked: aboutPage.about.openCredit(index)
            }
        }
    }

    SectionText {
        text: qsTr("Path of Exile is a trademark of Grinding Gear Games. This project isn't affiliated with or endorsed by them.")
    }
}
