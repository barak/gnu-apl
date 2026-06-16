; GNU APL Font installer/uninstaller (NSIS).
; Per-user install: no administrator rights required.
;
; Build with:  makensis GNU_APL_Font.nsi
; (or "make installer" from this directory, if nsis is installed)

!include "MUI2.nsh"

Name "GNU APL Font"
OutFile "GNU_APL_Font_Setup.exe"
RequestExecutionLevel user
Unicode true

InstallDir "$LOCALAPPDATA\GNU_APL_Font"

!define FONTS_DIR  "$LOCALAPPDATA\Microsoft\Windows\Fonts"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\GNU_APL_Font"
!define FONT_KEY   "Software\Microsoft\Windows NT\CurrentVersion\Fonts"
!define FONT_VALUE "GNU_APL (TrueType)"

;--------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "COPYRIGHT_NOTICE"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

;--------------------------------
Section "GNU APL Font" SecFont
    SetOutPath "${FONTS_DIR}"
    File "GNU_APL.ttf"

    WriteRegStr HKCU "${FONT_KEY}" "${FONT_VALUE}" "GNU_APL.ttf"

    SetOutPath "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr   HKCU "${UNINST_KEY}" "DisplayName"     "GNU APL Font"
    WriteRegStr   HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr   HKCU "${UNINST_KEY}" "Publisher"        "GNU APL Project"
    WriteRegStr   HKCU "${UNINST_KEY}" "DisplayVersion"   "1.0"
    WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1

    System::Call 'user32::SendMessageTimeoutA(i 0xffff, i ${WM_FONTCHANGE}, i 0, i 0, i 2, i 2000, *i .r0)'
SectionEnd

;--------------------------------
Section "Uninstall"
    Delete "${FONTS_DIR}\GNU_APL.ttf"
    DeleteRegValue HKCU "${FONT_KEY}" "${FONT_VALUE}"

    DeleteRegKey HKCU "${UNINST_KEY}"

    Delete "$INSTDIR\Uninstall.exe"
    RMDir  "$INSTDIR"

    System::Call 'user32::SendMessageTimeoutA(i 0xffff, i ${WM_FONTCHANGE}, i 0, i 0, i 2, i 2000, *i .r0)'
SectionEnd
