# SPDX-License-Identifier: MIT
#
# A Debian package of the build, made with cpack in the build folder, as the
# release builds do on Debian 13. dpkg-shlibdeps finds the libraries the app
# links, but not the QML modules and plugins it only loads when it runs, so
# those are named here. The overlay needs layer-shell-qt's Wayland plugin,
# which Debian keeps apart from the library the app links.

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

# a beta's is like 0.1.0~beta.2, which comes before 0.1.0
set(POU_PACKAGE_VERSION "${PROJECT_VERSION}" CACHE STRING "The version the package says")
set(CPACK_DEBIAN_PACKAGE_VERSION "${POU_PACKAGE_VERSION}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
set(CPACK_DEBIAN_PACKAGE_SECTION games)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# qt6-svg-plugins draws the icons, which are SVG, as the AppImage's does,
# and ca-certificates lets it trust the trade site, poe.ninja and GitHub
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qt6-wayland, layer-shell-qt, qt6-svg-plugins, ca-certificates, \
qml6-module-qtqml, qml6-module-qtqml-models, qml6-module-qtqml-workerscript, \
qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, \
qml6-module-qtquick-layouts, qml6-module-qtquick-shapes, qml6-module-qtquick-templates, qml6-module-qtquick-window, \
qml6-module-org-kde-kirigami, qml6-module-org-kde-kirigamiaddons-components, \
qml6-module-org-kde-kirigamiaddons-delegates, qml6-module-org-kde-kirigamiaddons-formcard, \
qml6-module-org-kde-kquickcontrols")
# the Plasma style, and Path of Building from Flathub
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qml6-module-org-kde-desktop, flatpak")
# to run another install of Path of Building
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "luajit")

# A license's text from where it starts, as the copyright file has it: each
# line indented a space, and a dot on the empty ones.
function(pou_debian_license file start out)
    file(READ "${file}" text)
    string(FIND "${text}" "${start}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "No \"${start}\" in ${file}")
    endif()
    string(SUBSTRING "${text}" ${at} -1 text)
    string(STRIP "${text}" text)
    string(REGEX REPLACE "[ \t]+\n" "\n" text "${text}")
    string(REPLACE "\n" "\n " text " ${text}")
    # again while there's one left, as two empty lines in a row share a newline
    string(FIND "${text}" "\n \n" at)
    while(NOT at EQUAL -1)
        string(REPLACE "\n \n" "\n .\n" text "${text}")
        string(FIND "${text}" "\n \n" at)
    endwhile()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# The holder from a license's "Copyright" line.
function(pou_debian_holder file out)
    file(STRINGS "${file}" line REGEX "^Copyright " LIMIT_COUNT 1)
    string(REGEX REPLACE "^Copyright (\\(c\\) )?" "" line "${line}")
    if(line STREQUAL "")
        message(FATAL_ERROR "No copyright line in ${file}")
    endif()
    set(${out} "${line}" PARENT_SCOPE)
endfunction()

# Debian looks for the licenses in /usr/share/doc/<package>/copyright, in its
# machine readable format. It's made from the licenses in the source, so they
# can't differ, and only the deb has it.
pou_debian_holder("${PROJECT_SOURCE_DIR}/LICENSE" POU_HOLDER)
pou_debian_holder("${PROJECT_SOURCE_DIR}/data/fonts/OFL-Spectral.txt" SPECTRAL_HOLDER)
pou_debian_holder("${PROJECT_SOURCE_DIR}/data/fonts/OFL-BigShoulders.txt" BIG_SHOULDERS_HOLDER)
pou_debian_license("${PROJECT_SOURCE_DIR}/LICENSE" "Permission is hereby granted" EXPAT_TEXT)
pou_debian_license("${PROJECT_SOURCE_DIR}/data/fonts/OFL-Spectral.txt" "SIL OPEN FONT LICENSE" OFL_TEXT)
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/debian/doc/copyright" @ONLY CONTENT
"Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: Path of Upgrading
Upstream-Contact: https://github.com/coreydeli/path-of-upgrading/issues
Source: https://github.com/coreydeli/path-of-upgrading

Files: *
Copyright: @POU_HOLDER@
License: Expat

Files: data/fonts/Spectral-*
Copyright: @SPECTRAL_HOLDER@
License: OFL-1.1

Files: data/fonts/BigShoulders*
Copyright: @BIG_SHOULDERS_HOLDER@
License: OFL-1.1

License: Expat
@EXPAT_TEXT@

License: OFL-1.1
@OFL_TEXT@
")
# Only cpack copies it in, so a plain install, which the AppImage is made
# from, leaves it out. It goes in /usr/share/doc whatever the prefix.
set(CPACK_INSTALLED_DIRECTORIES "${PROJECT_BINARY_DIR}/debian/doc;usr/share/doc/${CMAKE_PROJECT_NAME}")

include(CPack)
