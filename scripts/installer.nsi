; ==============================================================================
; SchoolCMS NSIS Setup Script
; Generates standard Windows installer wizard with Start Menu shortcuts.
; ==============================================================================

!define APP_NAME "SchoolCMS"
!define APP_VERSION "1.1.2"
!define APP_PUBLISHER "SchoolCMS Development Team"
!define APP_EXE "SchoolCMS.exe"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\dist\SchoolCMS_Setup_v${APP_VERSION}.exe"
InstallDir "$PROGRAMFILES64\SchoolCMS"
InstallDirRegKey HKLM "Software\SchoolCMS" "Install_Dir"
RequestExecutionLevel admin

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "SchoolCMS Core (required)"
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "..\dist\SchoolCMS-v1.1.2-windows-x64\*.*"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SchoolCMS" "DisplayName" "SchoolCMS - School Management System"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SchoolCMS" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SchoolCMS" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SchoolCMS" "NoRepair" 1
    WriteUninstaller "$INSTDIR\uninstall.exe"

    CreateDirectory "$SMPROGRAMS\SchoolCMS"
    CreateShortcut "$SMPROGRAMS\SchoolCMS\SchoolCMS.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
    CreateShortcut "$SMPROGRAMS\SchoolCMS\Uninstall SchoolCMS.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
SectionEnd

Section "Uninstall"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SchoolCMS"
    DeleteRegKey HKLM "Software\SchoolCMS"

    RMDir /r "$SMPROGRAMS\SchoolCMS"
    RMDir /r "$INSTDIR"
SectionEnd
