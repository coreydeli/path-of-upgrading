// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami

// The text color from the app's own scheme, not the desktop's palette.
T.Label {
    color: Kirigami.Theme.textColor
    linkColor: Kirigami.Theme.linkColor
}
