# SPDX-License-Identifier: MIT
#
# A Debian package of the build, made with cpack in the build folder, as the
# release builds do on Debian 13. dpkg-shlibdeps finds the libraries the app
# links, but not the QML modules it only loads when it runs, so those are named
# here.

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_VENDOR "Corey Deli")
set(CPACK_PACKAGE_CONTACT "Corey Deli <55706677+coreydeli@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
# or it's CMake's own text
set(CPACK_PACKAGE_DESCRIPTION "Press a hotkey over an item in Path of Exile 2 and a popup says \
whether it's an upgrade for your build, by gear score, by your stat weights, or by Path of \
Building once a build is imported. It can search the trade site for upgrades, price an item \
and find bases to craft on.")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES ON)

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
set(CPACK_DEBIAN_PACKAGE_SECTION games)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qt6-wayland, \
qml6-module-qtqml, qml6-module-qtqml-models, qml6-module-qtqml-workerscript, \
qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, \
qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window, \
qml6-module-org-kde-kirigami, qml6-module-org-kde-kirigamiaddons-components, \
qml6-module-org-kde-kirigamiaddons-delegates, qml6-module-org-kde-kirigamiaddons-formcard, \
qml6-module-org-kde-kquickcontrols")
# the Plasma style, and Path of Building from Flathub
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qml6-module-org-kde-desktop, flatpak")
# to run another install of Path of Building
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "luajit")

include(CPack)
