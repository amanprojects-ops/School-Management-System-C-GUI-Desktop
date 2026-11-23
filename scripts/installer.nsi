; ==============================================================================
; SchoolCMS NSIS Setup Script
; Generates standard Windows installer wizard with Start Menu shortcuts.
; ==============================================================================

!define APP_NAME "APCOACHING"
!define APP_VERSION "1.1.3"
!define APP_PUBLISHER "SchoolCMS Development Team"
!define APP_EXE "APCOACHING.exe"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\dist\APCOACHING_Setup_v${APP_VERSION}.exe"
InstallDir "$PROGRAMFILES64\APCOACHING"
InstallDirRegKey HKLM "Software\APCOACHING" "Install_Dir"
RequestExecutionLevel admin

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "SchoolCMS Core (required)"
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "..\dist\APCOACHING-v1.1.3-windows-x64\*.*"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\APCOACHING" "DisplayName" "SchoolCMS - School Management System"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\APCOACHING" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\APCOACHING" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\APCOACHING" "NoRepair" 1
    WriteUninstaller "$INSTDIR\uninstall.exe"

    CreateDirectory "$SMPROGRAMS\APCOACHING"
    CreateShortcut "$SMPROGRAMS\APCOACHING\APCOACHING.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
    CreateShortcut "$SMPROGRAMS\APCOACHING\Uninstall SchoolCMS.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
SectionEnd

Section "Uninstall"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\APCOACHING"
    DeleteRegKey HKLM "Software\APCOACHING"

    RMDir /r "$SMPROGRAMS\APCOACHING"
    RMDir /r "$INSTDIR"
SectionEnd
