; ==============================================================================
; SchoolCMS / APCOACHING Windows Installer Wizard (NSIS Script)
; Modern UI 2 with Desktop & Start Menu Shortcuts, Full Uninstaller & Registry
; ==============================================================================

!ifndef APP_NAME
  !define APP_NAME "APCOACHING"
!endif

!ifndef APP_VERSION
  !define APP_VERSION "2.0.0"
!endif

!ifndef APP_PUBLISHER
  !define APP_PUBLISHER "APCOACHING Team"
!endif

!ifndef APP_EXE
  !define APP_EXE "${APP_NAME}.exe"
!endif

!ifndef SOURCE_DIR
  !define SOURCE_DIR "..\dist\${APP_NAME}-v${APP_VERSION}-windows-x64"
!endif

!ifndef OUTPUT_SETUP
  !define OUTPUT_SETUP "..\dist\${APP_NAME}-v${APP_VERSION}-Setup.exe"
!endif

; ------------------------------------------------------------------------------
; General Configuration
; ------------------------------------------------------------------------------
Name "${APP_NAME} v${APP_VERSION}"
OutFile "${OUTPUT_SETUP}"
Unicode True
SetCompressor /SOLID lzma

; Per-user install allows installation without requiring UAC Admin privileges
RequestExecutionLevel user

; Default installation folder: %LOCALAPPDATA%\Programs\APCOACHING
InstallDir "$LOCALAPPDATA\Programs\${APP_NAME}"
InstallDirRegKey HKCU "Software\${APP_NAME}" "InstallPath"

; ------------------------------------------------------------------------------
; Modern UI 2 Configuration
; ------------------------------------------------------------------------------
!include "MUI2.nsh"
!include "FileFunc.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "..\assets\icons\app.ico"
!define MUI_UNICON "..\assets\icons\app.ico"

; Welcome Page
!define MUI_WELCOMEPAGE_TITLE "Welcome to ${APP_NAME} Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT "This wizard will install ${APP_NAME} v${APP_VERSION} on your computer.\r\n\r\nHigh-Performance Pure C11 & GTK4 School Management System.\r\n\r\nClick Next to continue."
!insertmacro MUI_PAGE_WELCOME

; Directory Selection Page
!define MUI_DIRECTORYPAGE_TEXT_TOP "Setup will install ${APP_NAME} in the following folder. To install in a different folder, click Browse and select another folder."
!insertmacro MUI_PAGE_DIRECTORY

; Installation Progress Page
!insertmacro MUI_PAGE_INSTFILES

; Finish Page
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${APP_NAME} now"
!define MUI_FINISHPAGE_LINK "Visit SchoolCMS Project"
!define MUI_FINISHPAGE_LINK_LOCATION "https://github.com/amanprojects-ops"
!insertmacro MUI_PAGE_FINISH

; Uninstaller Pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"

; ------------------------------------------------------------------------------
; Installer Section
; ------------------------------------------------------------------------------
Section "Install Main Files" SecMain
    SetOutPath "$INSTDIR"

    ; Copy all packaged distribution files
    File /r "${SOURCE_DIR}\*.*"

    ; Store install location in registry
    WriteRegStr HKCU "Software\${APP_NAME}" "InstallPath" "$INSTDIR"

    ; Create Shortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\assets\icons\app.ico" 0
    CreateShortCut "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\assets\icons\app.ico" 0
    CreateShortCut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\assets\icons\app.ico" 0

    ; Create Uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Register in Windows Add/Remove Programs (Settings > Installed Apps)
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME} — School Management System"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${APP_PUBLISHER}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayIcon" "$INSTDIR\assets\icons\app.ico"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "InstallLocation" "$INSTDIR"
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoRepair" 1

    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "EstimatedSize" "$0"
SectionEnd

; ------------------------------------------------------------------------------
; Uninstaller Section
; ------------------------------------------------------------------------------
Section "Uninstall"
    ; Remove desktop and start menu shortcuts
    Delete "$DESKTOP\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk"
    RMDir "$SMPROGRAMS\${APP_NAME}"

    ; Remove installed folders
    RMDir /r "$INSTDIR\assets"
    RMDir /r "$INSTDIR\share"
    RMDir /r "$INSTDIR\lib"
    RMDir /r "$INSTDIR\backups"

    ; Remove files
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\*.exe"
    Delete "$INSTDIR\*.bat"
    Delete "$INSTDIR\*.txt"
    Delete "$INSTDIR\schoolcms.db"

    ; Remove root folder
    RMDir /r "$INSTDIR"

    ; Clean registry keys
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
    DeleteRegKey HKCU "Software\${APP_NAME}"
SectionEnd
