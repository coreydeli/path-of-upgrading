; SPDX-License-Identifier: MIT
;
; The Windows installer, which the release builds make with
; makensis -DVERSION=0.1.0 -DLABEL=0.1.0-beta.1 -DAPP=<the app's folder> -DOUTFILE=<installer.exe>
; It installs for you alone, without asking for an administrator, in
; AppData\Local\Programs, with a Start menu entry and an uninstaller.

Unicode true
!include "MUI2.nsh"

!define NAME "Path of Upgrading"
!define PROGRAM "path-of-upgrading.exe"
!define UNINSTALL "Software\Microsoft\Windows\CurrentVersion\Uninstall\path-of-upgrading"
; where the app's Start with Windows switch puts it
!define RUN "Software\Microsoft\Windows\CurrentVersion\Run"

Name "${NAME}"
OutFile "${OUTFILE}"
; always its own folder, which the uninstaller removes as a whole
InstallDir "$LOCALAPPDATA\Programs\${NAME}"
RequestExecutionLevel user
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${NAME}"
VIAddVersionKey "ProductVersion" "${LABEL}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${NAME} installer"
VIAddVersionKey "LegalCopyright" "Corey Deli, MIT license"

!define MUI_ICON "path-of-upgrading.ico"
!define MUI_UNICON "path-of-upgrading.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${PROGRAM}"
!define MUI_FINISHPAGE_RUN_TEXT "Start ${NAME}"

!insertmacro MUI_PAGE_LICENSE "${APP}\LICENSE.txt"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section
    ; what an older version had that this one doesn't
    RMDir /r "$INSTDIR"
    SetOutPath "$INSTDIR"
    File /r "${APP}\*"
    WriteUninstaller "$INSTDIR\uninstall.exe"
    CreateShortcut "$SMPROGRAMS\${NAME}.lnk" "$INSTDIR\${PROGRAM}"

    WriteRegStr HKCU "${UNINSTALL}" "DisplayName" "${NAME}"
    WriteRegStr HKCU "${UNINSTALL}" "DisplayVersion" "${LABEL}"
    WriteRegStr HKCU "${UNINSTALL}" "Publisher" "Corey Deli"
    WriteRegStr HKCU "${UNINSTALL}" "DisplayIcon" "$INSTDIR\${PROGRAM}"
    WriteRegStr HKCU "${UNINSTALL}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINSTALL}" "URLInfoAbout" "https://github.com/coreydeli/path-of-upgrading"
    WriteRegStr HKCU "${UNINSTALL}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKCU "${UNINSTALL}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINSTALL}" "NoRepair" 1
SectionEnd

Section "Uninstall"
    Delete "$SMPROGRAMS\${NAME}.lnk"
    DeleteRegValue HKCU "${RUN}" "${NAME}"
    DeleteRegKey HKCU "${UNINSTALL}"
    ; your characters and settings stay, in AppData
    RMDir /r "$INSTDIR"
SectionEnd
