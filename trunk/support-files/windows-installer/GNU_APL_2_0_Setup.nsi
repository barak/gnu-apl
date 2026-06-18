; GNU APL installer/uninstaller for Windows (NSIS).
;
; Component page shows two top-level items (radio-button behaviour):
;   [✓] Full install      — checked by default; Custom children deselected
;   [ ] Custom install    — SectionGroup; when checked, Full is deselected
;       [ ] GNU APL interpreter
;       [ ] Library files
;       [ ] GNU APL font
;       [ ] APL keyboard layout file (APLUSEN.klc for MSKLC)
;       [ ] Desktop shortcut
;
; Mutual exclusion is implemented via $Mode ("F"/"C") and $WasCustomSelected.
; SecFull carries all install code.  Custom children carry the same code via
; macros so code is never duplicated, and run only when their checkbox is on.
;
; Per-user install: no administrator rights required.
; Install location: %USERPROFILE%\gnu-apl.d  ($PROFILE\gnu-apl.d in NSIS).
;
; Build:  make windows-installer  (from the top-level source directory)

!include "MUI2.nsh"
!include "Sections.nsh"
!include "LogicLib.nsh"

!ifndef APL_VERSION
  !define APL_VERSION "0.0"
!endif
!ifndef APL_VERSION_US
  !define APL_VERSION_US "0_0"
!endif

Name "GNU APL"
OutFile "GNU_APL_${APL_VERSION_US}_Setup.exe"
RequestExecutionLevel user
Unicode true

InstallDir "$PROFILE\gnu-apl.d"

!define FONTS_DIR  "$LOCALAPPDATA\Microsoft\Windows\Fonts"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\GNU_APL"
!define FONT_KEY   "Software\Microsoft\Windows NT\CurrentVersion\Fonts"
!define FONT_VALUE "GNU_APL (TrueType)"
!define STAGE      "stage\usr\local\lib\apl"
!define ETCSTAGE   "stage\usr\local\etc"

Var Mode              ; "F" = Full, "C" = Custom
Var WasCustomSelected ; "1" if SecCustom group was selected in previous call

;--------------------------------
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\COPYING"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Install macros — shared between SecFull and the Custom children so that
; install code is never duplicated.

!macro M_Core
    SetOutPath "$INSTDIR"
    File "apl-${APL_VERSION}.exe"
    File "/usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll"
    File "gnu_apl.ico"
    CreateDirectory "$SMPROGRAMS\GNU APL"
    CreateShortCut "$SMPROGRAMS\GNU APL\GNU APL.lnk" \
                   "$INSTDIR\apl-${APL_VERSION}.exe" "" \
                   "$INSTDIR\gnu_apl.ico" 0 SW_SHOWNORMAL "" "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr   HKCU "${UNINST_KEY}" "DisplayName"     "GNU APL"
    WriteRegStr   HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr   HKCU "${UNINST_KEY}" "Publisher"       "GNU APL Project"
    WriteRegStr   HKCU "${UNINST_KEY}" "DisplayVersion"  "${APL_VERSION}"
    WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
!macroend

!macro M_Libs
    SetOutPath "$INSTDIR"
    File "${ETCSTAGE}\preferences"
    SetOutPath "$INSTDIR\workspaces"
    File "${STAGE}\workspaces\APL_CGI.apl"
    File "${STAGE}\workspaces\RUBIK.apl"
    File "${STAGE}\workspaces\sudoku.apl"
    File "${STAGE}\workspaces\sudoku_DLX.apl"
    SetOutPath "$INSTDIR\wslib1"
    SetOutPath "$INSTDIR\wslib2"
    SetOutPath "$INSTDIR\wslib6"
    SetOutPath "$INSTDIR\wslib7"
    SetOutPath "$INSTDIR\wslib8"
    SetOutPath "$INSTDIR\wslib9"
    SetOutPath "$INSTDIR\wslib3"
    File "${STAGE}\wslib3\meta.apl"
    SetOutPath "$INSTDIR\wslib3\DALY"
    File "${STAGE}\wslib3\DALY\*.*"
    SetOutPath "$INSTDIR\wslib4"
    SetOutPath "$INSTDIR\wslib5"
    File "${STAGE}\wslib5\ASSERT.apl"
    File "${STAGE}\wslib5\HTML.apl"
    SetOutPath "$INSTDIR\wslib5\DALY"
    File "${STAGE}\wslib5\DALY\*.*"
    SetOutPath "$INSTDIR\wslib5\APLComponentFiles"
    File "${STAGE}\wslib5\APLComponentFiles\*.*"
    SetOutPath "$INSTDIR\wslib5\iso-apl-cf"
    File "${STAGE}\wslib5\iso-apl-cf\*.*"
!macroend

!macro M_Font
    SetOutPath "${FONTS_DIR}"
    File "..\fonts\GNU_APL.ttf"
    WriteRegStr HKCU "${FONT_KEY}" "${FONT_VALUE}" "GNU_APL.ttf"
    SetOutPath "$INSTDIR"
    File "..\fonts\COPYRIGHT_NOTICE"
    System::Call 'user32::SendMessageTimeoutA(i 0xffff, i ${WM_FONTCHANGE}, i 0, i 0, i 2, i 2000, *i .r0)'
!macroend

!macro M_Keyb
    SetOutPath "$INSTDIR"
    File "APLUSEN.klc"
!macroend

!macro M_Desk
    CreateShortCut "$DESKTOP\GNU APL.lnk" \
                   "$INSTDIR\apl-${APL_VERSION}.exe" "" \
                   "$INSTDIR\gnu_apl.ico" 0 SW_SHOWNORMAL "" "$INSTDIR"
!macroend

;--------------------------------
; Section layout
;
;  SecFull        — "Full install"; has all install code; children deselected
;  SecCustom      — SectionGroup "Custom install"; children individually selectable
;    SecCore      — interpreter
;    SecLibs      — library files
;    SecFont      — font
;    SecKeyb      — keyboard layout file
;    SecDesk      — desktop shortcut

Section "Full install" SecFull
    ; Runs only when SecFull is checked (Full mode); children are then deselected.
    !insertmacro M_Core
    !insertmacro M_Libs
    !insertmacro M_Font
    !insertmacro M_Keyb
    !insertmacro M_Desk
SectionEnd

SectionGroup /e "Custom install" SecCustom

    Section "GNU APL interpreter" SecCore
        !insertmacro M_Core
    SectionEnd

    Section "Library files" SecLibs
        !insertmacro M_Libs
    SectionEnd

    Section "GNU APL font" SecFont
        !insertmacro M_Font
    SectionEnd

    Section "APL keyboard layout (MSKLC source)" SecKeyb
        !insertmacro M_Keyb
    SectionEnd

    Section "Desktop shortcut" SecDesk
        !insertmacro M_Desk
    SectionEnd

SectionGroupEnd

;--------------------------------
; Mode-change helpers.

Function EnterFull
    StrCpy $Mode "F"
    SectionSetFlags ${SecCore}   0
    SectionSetFlags ${SecLibs}   0
    SectionSetFlags ${SecFont}   0
    SectionSetFlags ${SecKeyb}   0
    SectionSetFlags ${SecDesk}   0
    SectionSetFlags ${SecCustom} 0   ; deselect group header
FunctionEnd

Function EnterCustom
    StrCpy $Mode "C"
    SectionSetFlags ${SecCore}   ${SF_SELECTED}
    SectionSetFlags ${SecLibs}   ${SF_SELECTED}
    SectionSetFlags ${SecFont}   ${SF_SELECTED}
    SectionSetFlags ${SecKeyb}   ${SF_SELECTED}
    SectionSetFlags ${SecDesk}   ${SF_SELECTED}
    SectionSetFlags ${SecCustom} ${SF_SELECTED}  ; check group header
FunctionEnd

;--------------------------------
; .onInit — start in Full-install mode.

Function .onInit
    SectionSetFlags ${SecFull}   ${SF_SELECTED}
    SectionSetFlags ${SecCustom} 0
    SectionSetFlags ${SecCore}   0
    SectionSetFlags ${SecLibs}   0
    SectionSetFlags ${SecFont}   0
    SectionSetFlags ${SecKeyb}   0
    SectionSetFlags ${SecDesk}   0
    StrCpy $Mode              "F"
    StrCpy $WasCustomSelected "0"
FunctionEnd

;--------------------------------
; .onSelChange — enforce Full / Custom mutual exclusion.
;
; State machine:
;   Mode == "F":  Full is on; Custom children are deselected.
;     • User unchecks Full         → EnterCustom (pre-selects all children)
;     • User checks Custom group   → deselect Full, EnterCustom
;   Mode == "C":  Custom is on; children individually selectable.
;     • User checks Full           → EnterFull (deselects all children)

Function .onSelChange
    Push $R0
    Push $R1

    SectionGetFlags ${SecFull}   $R0
    IntOp           $R0 $R0 & ${SF_SELECTED}

    SectionGetFlags ${SecCustom} $R1
    IntOp           $R1 $R1 & ${SF_SELECTED}

    ${If} $Mode == "F"
        ${If} $R0 != ${SF_SELECTED}
            ; Full was unchecked → Custom mode
            Call EnterCustom
        ${ElseIf} $R1 == ${SF_SELECTED}
        ${AndIf}  $WasCustomSelected != "1"
            ; Custom group was just checked while Full was on → Custom mode
            SectionSetFlags ${SecFull} 0
            Call EnterCustom
        ${EndIf}
    ${Else}
        ${If} $R0 == ${SF_SELECTED}
            ; Full was checked → Full mode
            Call EnterFull
        ${EndIf}
    ${EndIf}

    ; Re-read SecCustom after potential modifications and save for next call.
    SectionGetFlags ${SecCustom} $R1
    IntOp           $R1 $R1 & ${SF_SELECTED}
    StrCpy $WasCustomSelected $R1

    Pop $R1
    Pop $R0
FunctionEnd

;--------------------------------
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecFull} \
        "Install all components in one step.  Deselect to choose \
individual components below."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecCustom} \
        "Choose which components to install.  Check this group (or \
any child) to enter custom mode; Full install is then deselected."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecCore} \
        "The GNU APL interpreter (apl-${APL_VERSION}.exe), the required \
runtime DLL, and a Start Menu shortcut."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecLibs} \
        "Workspace library files (wslib1-9, workspaces) and the default \
preferences file.  Required for )COPY, )LIB, and most APL examples."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecFont} \
        "The GNU APL TrueType font, registered for the current user.  \
Provides correct display of all APL glyphs in cmd.exe."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecKeyb} \
        "APLUSEN.klc: source file for Microsoft Keyboard Layout Creator \
(MSKLC).  Open it in MSKLC, choose Build -> Build DLL and Setup Package, \
then run the generated setup to install an AltGr-based APL keyboard layout."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesk} \
        "Create a GNU APL shortcut on the Desktop, using the striped \
GNU APL icon."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
Section "Uninstall"
    Delete "${FONTS_DIR}\GNU_APL.ttf"
    DeleteRegValue HKCU "${FONT_KEY}" "${FONT_VALUE}"
    Delete "$DESKTOP\GNU APL.lnk"
    Delete "$SMPROGRAMS\GNU APL\GNU APL.lnk"
    RMDir  "$SMPROGRAMS\GNU APL"
    DeleteRegKey HKCU "${UNINST_KEY}"
    RMDir /r "$INSTDIR"
    System::Call 'user32::SendMessageTimeoutA(i 0xffff, i ${WM_FONTCHANGE}, i 0, i 0, i 2, i 2000, *i .r0)'
SectionEnd
