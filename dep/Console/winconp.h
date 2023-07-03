
#ifndef _WINCONP_
#define _WINCONP_

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if _MSC_VER >= 1200
#pragma warning(push)
#pragma warning(disable:4820) // padding added after data member
#endif

#include <wincontypes.h>

//
// History flags (internal)
//

#define CHI_VALID_FLAGS (HISTORY_NO_DUP_FLAG)

//
// Selection flags (internal)
//

#define CONSOLE_SELECTION_INVERTED      0x0010   // selection is inverted (turned off)
#define CONSOLE_SELECTION_VALID         (CONSOLE_SELECTION_IN_PROGRESS | \
                                         CONSOLE_SELECTION_NOT_EMPTY | \
                                         CONSOLE_MOUSE_SELECTION | \
                                         CONSOLE_MOUSE_DOWN)


WINBASEAPI
BOOL
WINAPI
GetConsoleKeyboardLayoutNameA(
    _Out_writes_(KL_NAMELENGTH) LPSTR pszLayout);
WINBASEAPI
BOOL
WINAPI
GetConsoleKeyboardLayoutNameW(
    _Out_writes_(KL_NAMELENGTH) LPWSTR pszLayout);
#ifdef UNICODE
#define GetConsoleKeyboardLayoutName  GetConsoleKeyboardLayoutNameW
#else
#define GetConsoleKeyboardLayoutName  GetConsoleKeyboardLayoutNameA
#endif // !UNICODE

//
// Registry strings
//

#define CONSOLE_REGISTRY_STRING                         L"Console"
#define CONSOLE_REGISTRY_FONTSIZE                       L"FontSize"
#define CONSOLE_REGISTRY_FONTFAMILY                     L"FontFamily"
#define CONSOLE_REGISTRY_BUFFERSIZE                     L"ScreenBufferSize"
#define CONSOLE_REGISTRY_CURSORSIZE                     L"CursorSize"
#define CONSOLE_REGISTRY_WINDOWMAXIMIZED                L"WindowMaximized"
#define CONSOLE_REGISTRY_WINDOWSIZE                     L"WindowSize"
#define CONSOLE_REGISTRY_WINDOWPOS                      L"WindowPosition"
#define CONSOLE_REGISTRY_WINDOWALPHA                    L"WindowAlpha"
#define CONSOLE_REGISTRY_FILLATTR                       L"ScreenColors"
#define CONSOLE_REGISTRY_POPUPATTR                      L"PopupColors"
#define CONSOLE_REGISTRY_FULLSCR                        L"FullScreen"
#define CONSOLE_REGISTRY_QUICKEDIT                      L"QuickEdit"
#define CONSOLE_REGISTRY_FACENAME                       L"FaceName"
#define CONSOLE_REGISTRY_FONTWEIGHT                     L"FontWeight"
#define CONSOLE_REGISTRY_INSERTMODE                     L"InsertMode"
#define CONSOLE_REGISTRY_HISTORYSIZE                    L"HistoryBufferSize"
#define CONSOLE_REGISTRY_HISTORYBUFS                    L"NumberOfHistoryBuffers"
#define CONSOLE_REGISTRY_HISTORYNODUP                   L"HistoryNoDup"
#define CONSOLE_REGISTRY_COLORTABLE                     L"ColorTable%02u"
#define CONSOLE_REGISTRY_EXTENDEDEDITKEY                L"ExtendedEditKey"
#define CONSOLE_REGISTRY_EXTENDEDEDITKEY_CUSTOM         L"ExtendedEditkeyCustom"
#define CONSOLE_REGISTRY_WORD_DELIM                     L"WordDelimiters"
#define CONSOLE_REGISTRY_TRIMZEROHEADINGS               L"TrimLeadingZeros"
#define CONSOLE_REGISTRY_LOAD_CONIME                    L"LoadConIme"
#define CONSOLE_REGISTRY_ENABLE_COLOR_SELECTION         L"EnableColorSelection"
#define CONSOLE_REGISTRY_SCROLLSCALE                    L"ScrollScale"

// V2 console settings
#define CONSOLE_REGISTRY_FORCEV2                        L"ForceV2"
#define CONSOLE_REGISTRY_LINESELECTION                  L"LineSelection"
#define CONSOLE_REGISTRY_FILTERONPASTE                  L"FilterOnPaste"
#define CONSOLE_REGISTRY_LINEWRAP                       L"LineWrap"
#define CONSOLE_REGISTRY_CTRLKEYSHORTCUTS_DISABLED      L"CtrlKeyShortcutsDisabled"
#define CONSOLE_REGISTRY_ALLOW_ALTF4_CLOSE              L"AllowAltF4Close"
#define CONSOLE_REGISTRY_VIRTTERM_LEVEL                 L"VirtualTerminalLevel"

#define CONSOLE_REGISTRY_CURSORTYPE                     L"CursorType"
#define CONSOLE_REGISTRY_CURSORCOLOR                    L"CursorColor"

#define CONSOLE_REGISTRY_INTERCEPTCOPYPASTE             L"InterceptCopyPaste"

#define CONSOLE_REGISTRY_COPYCOLOR                      L"CopyColor"
#define CONSOLE_REGISTRY_USEDX                          L"UseDx"

#define CONSOLE_REGISTRY_DEFAULTFOREGROUND             L"DefaultForeground"
#define CONSOLE_REGISTRY_DEFAULTBACKGROUND             L"DefaultBackground"
#define CONSOLE_REGISTRY_TERMINALSCROLLING             L"TerminalScrolling"
// end V2 console settings

    /*
     * Starting code page
     */
#define CONSOLE_REGISTRY_CODEPAGE    (L"CodePage")

//
// registry strings on HKEY_LOCAL_MACHINE
//
#define MACHINE_REGISTRY_CONSOLE        (L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Console")
#define MACHINE_REGISTRY_CONSOLEIME     (L"ConsoleIME")
#define MACHINE_REGISTRY_ENABLE_CONIME_ON_SYSTEM_PROCESS     (L"EnableConImeOnSystemProcess")


#define MACHINE_REGISTRY_CONSOLE_TTFONT (L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Console\\TrueTypeFont")
#define MACHINE_REGISTRY_CONSOLE_TTFONT_WIN32_PATH (L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Console\\TrueTypeFont")


#define MACHINE_REGISTRY_CONSOLE_NLS    (L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Console\\Nls")


#define MACHINE_REGISTRY_CONSOLE_FULLSCREEN (L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Console\\FullScreen")
#define MACHINE_REGISTRY_INITIAL_PALETTE           (L"InitialPalette")
#define MACHINE_REGISTRY_COLOR_BUFFER              (L"ColorBuffer")
#define MACHINE_REGISTRY_COLOR_BUFFER_NO_TRANSLATE (L"ColorBufferNoTranslate")
#define MACHINE_REGISTRY_MODE_FONT_PAIRS           (L"ModeFontPairs")
#define MACHINE_REGISTRY_FS_CODEPAGE               (L"CodePage")


#define MACHINE_REGISTRY_EUDC    (L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Nls\\CodePage\\EUDCCodeRange")


//
// TrueType font list
//

// doesn't available bold when add BOLD_MARK on first of face name.
#define BOLD_MARK    (L'*')

typedef struct _TT_FONT_LIST {
    SINGLE_LIST_ENTRY List;
    UINT  CodePage;
    BOOL  fDisableBold;
    TCHAR FaceName1[LF_FACESIZE];
    TCHAR FaceName2[LF_FACESIZE];
} TTFONTLIST, *LPTTFONTLIST;

//
// registry strings on HKEY_CURRENT_USER
//
#define PRELOAD_REGISTRY_STRING      (L"Keyboard Layout\\Preload")


//
// Special key for previous word erase
//
#define EXTKEY_ERASE_PREV_WORD  (0x7f)

#ifndef NOGDI

typedef struct _CONSOLE_GRAPHICS_BUFFER_INFO {
    DWORD dwBitMapInfoLength;
    LPBITMAPINFO lpBitMapInfo;
    DWORD dwUsage;
    HANDLE hMutex;
    PVOID lpBitMap;
} CONSOLE_GRAPHICS_BUFFER_INFO, *PCONSOLE_GRAPHICS_BUFFER_INFO;

#endif

BOOL
WINAPI
InvalidateConsoleDIBits(
    _In_ HANDLE hConsoleOutput,
    _In_ PSMALL_RECT lpRect);

VOID
WINAPI
SetLastConsoleEventActive(
    VOID);

#define VDM_HIDE_WINDOW         1
#define VDM_IS_ICONIC           2
#define VDM_CLIENT_RECT         3
#define VDM_CLIENT_TO_SCREEN    4
#define VDM_SCREEN_TO_CLIENT    5
#define VDM_IS_HIDDEN           6
#define VDM_FULLSCREEN_NOPAINT  7
#define VDM_SET_VIDEO_MODE      8

BOOL
WINAPI
VDMConsoleOperation(
    _In_ DWORD iFunction,
    _Inout_opt_ LPVOID lpData);


BOOL
WINAPI
SetConsoleIcon(
    _In_ HICON hIcon);

//
// These console font APIs don't appear to be used anywhere. Maybe they
// should be removed.
//

BOOL
WINAPI
SetConsoleFont(
    _In_ HANDLE hConsoleOutput,
    _In_ DWORD nFont);

DWORD
WINAPI
GetConsoleFontInfo(
    _In_ HANDLE hConsoleOutput,
    _In_ BOOL bMaximumWindow,
    _In_ DWORD nLength,
    _Out_ PCONSOLE_FONT_INFO lpConsoleFontInfo);

DWORD
WINAPI
GetNumberOfConsoleFonts(
    VOID);

BOOL
WINAPI
SetConsoleCursor(
    _In_ HANDLE hConsoleOutput,
    _In_ HCURSOR hCursor);

int
WINAPI
ShowConsoleCursor(
    _In_ HANDLE hConsoleOutput,
    _In_ BOOL bShow);

HMENU
APIENTRY
ConsoleMenuControl(
    _In_ HANDLE hConsoleOutput,
    _In_ UINT dwCommandIdLow,
    _In_ UINT dwCommandIdHigh);

BOOL
SetConsolePalette(
    _In_ HANDLE hConsoleOutput,
    _In_ HPALETTE hPalette,
    _In_ UINT dwUsage);

#define CONSOLE_UNREGISTER_VDM 0
#define CONSOLE_REGISTER_VDM   1
#define CONSOLE_REGISTER_WOW   2

BOOL
APIENTRY
RegisterConsoleVDM(
    _In_ DWORD dwRegisterFlags,
    _In_ HANDLE hStartHardwareEvent,
    _In_ HANDLE hEndHardwareEvent,
    _In_ HANDLE hErrorhardwareEvent,
    _Reserved_ DWORD Reserved,
    _Out_ LPDWORD lpStateLength,
    _Outptr_ PVOID *lpState,
    _In_opt_ COORD VDMBufferSize,
    _Outptr_ PVOID *lpVDMBuffer);

BOOL
APIENTRY
GetConsoleHardwareState(
    _In_ HANDLE hConsoleOutput,
    _Out_ PCOORD lpResolution,
    _Out_ PCOORD lpFontSize);

BOOL
APIENTRY
SetConsoleHardwareState(
    _In_ HANDLE hConsoleOutput,
    _In_ COORD dwResolution,
    _In_ COORD dwFontSize);

#define CONSOLE_NOSHORTCUTKEY   0x00000000        /* no shortcut key  */
#define CONSOLE_ALTTAB          0x00000001        /* Alt + Tab        */
#define CONSOLE_ALTESC          0x00000002        /* Alt + Escape     */
#define CONSOLE_ALTSPACE        0x00000004        /* Alt + Space      */
#define CONSOLE_ALTENTER        0x00000008        /* Alt + Enter      */
#define CONSOLE_ALTPRTSC        0x00000010        /* Alt Print screen */
#define CONSOLE_PRTSC           0x00000020        /* Print screen     */
#define CONSOLE_CTRLESC         0x00000040        /* Ctrl + Escape    */

typedef struct _APPKEY {
    WORD Modifier;
    WORD ScanCode;
} APPKEY, *LPAPPKEY;

#define CONSOLE_MODIFIER_SHIFT      0x0003   // Left shift key
#define CONSOLE_MODIFIER_CONTROL    0x0004   // Either Control shift key
#define CONSOLE_MODIFIER_ALT        0x0008   // Either Alt shift key

BOOL
APIENTRY
SetConsoleKeyShortcuts(
    _In_ BOOL bSet,
    _In_ BYTE bReserveKeys,
    _In_reads_(dwNumAppKeys) LPAPPKEY lpAppKeys,
    _In_ DWORD dwNumAppKeys);

BOOL
APIENTRY
SetConsoleMenuClose(
    _In_ BOOL bEnable);

DWORD
GetConsoleInputExeNameA(
    _In_ DWORD nBufferLength,
    _Out_writes_(nBufferLength) LPSTR lpBuffer);
DWORD
GetConsoleInputExeNameW(
    _In_ DWORD nBufferLength,
    _Out_writes_(nBufferLength) LPWSTR lpBuffer);
#ifdef UNICODE
#define GetConsoleInputExeName  GetConsoleInputExeNameW
#else
#define GetConsoleInputExeName  GetConsoleInputExeNameA
#endif // !UNICODE

BOOL
SetConsoleInputExeNameA(
    _In_ LPSTR lpExeName);
BOOL
SetConsoleInputExeNameW(
    _In_ LPWSTR lpExeName);
#ifdef UNICODE
#define SetConsoleInputExeName  SetConsoleInputExeNameW
#else
#define SetConsoleInputExeName  SetConsoleInputExeNameA
#endif // !UNICODE

BOOL
WINAPI
ReadConsoleInputExA(
    _In_ HANDLE hConsoleInput,
    _Out_writes_(nLength) PINPUT_RECORD lpBuffer,
    _In_ DWORD nLength,
    _Out_ LPDWORD lpNumberOfEventsRead,
    _In_ USHORT wFlags);
BOOL
WINAPI
ReadConsoleInputExW(
    _In_ HANDLE hConsoleInput,
    _Out_writes_(nLength) PINPUT_RECORD lpBuffer,
    _In_ DWORD nLength,
    _Out_ LPDWORD lpNumberOfEventsRead,
    _In_ USHORT wFlags);
#ifdef UNICODE
#define ReadConsoleInputEx  ReadConsoleInputExW
#else
#define ReadConsoleInputEx  ReadConsoleInputExA
#endif // !UNICODE

BOOL
WINAPI
WriteConsoleInputVDMA(
    _In_ HANDLE hConsoleInput,
    _In_reads_(nLength) PINPUT_RECORD lpBuffer,
    _In_ DWORD nLength,
    _Out_ LPDWORD lpNumberOfEventsWritten);
BOOL
WINAPI
WriteConsoleInputVDMW(
    _In_ HANDLE hConsoleInput,
    _In_reads_(nLength) PINPUT_RECORD lpBuffer,
    _In_ DWORD nLength,
    _Out_ LPDWORD lpNumberOfEventsWritten);
#ifdef UNICODE
#define WriteConsoleInputVDM  WriteConsoleInputVDMW
#else
#define WriteConsoleInputVDM  WriteConsoleInputVDMA
#endif // !UNICODE


BOOL
APIENTRY
GetConsoleNlsMode(
    _In_ HANDLE hConsole,
    _Out_ PDWORD lpdwNlsMode);

BOOL
APIENTRY
SetConsoleNlsMode(
    _In_ HANDLE hConsole,
    _In_ DWORD fdwNlsMode);

BOOL
APIENTRY
GetConsoleCharType(
    _In_ HANDLE hConsole,
    _In_ COORD coordCheck,
    _Out_ PDWORD pdwType);

#define CHAR_TYPE_SBCS     0   // Displayed SBCS character
#define CHAR_TYPE_LEADING  2   // Displayed leading byte of DBCS
#define CHAR_TYPE_TRAILING 3   // Displayed trailing byte of DBCS

BOOL
APIENTRY
SetConsoleLocalEUDC(
    _In_ HANDLE hConsoleHandle,
    _In_ WORD   wCodePoint,
    _In_ COORD  cFontSize,
    _In_ PCHAR  lpSB);

BOOL
APIENTRY
SetConsoleCursorMode(
    _In_ HANDLE hConsoleHandle,
    _In_ BOOL   Blink,
    _In_ BOOL   DBEnable);

BOOL
APIENTRY
GetConsoleCursorMode(
    _In_ HANDLE hConsoleHandle,
    _Out_ PBOOL  pbBlink,
    _Out_ PBOOL  pbDBEnable);

BOOL
APIENTRY
RegisterConsoleOS2(
    _In_ BOOL fOs2Register);

BOOL
APIENTRY
SetConsoleOS2OemFormat(
    _In_ BOOL fOs2OemFormat);

BOOL
IsConsoleFullWidth(
    _In_ HDC hDC,
    _In_ DWORD CodePage,
    _In_ WCHAR wch);

#if defined(FE_IME)
BOOL
APIENTRY
RegisterConsoleIME(
    _In_ HWND  hWndConsoleIME,
    _Out_opt_ DWORD *lpdwConsoleThreadId);

BOOL
APIENTRY
UnregisterConsoleIME(
    VOID);
#endif // FE_IME

//
// These bits are always on for console handles and are used for routing
// by windows.
//

#define CONSOLE_HANDLE_SIGNATURE 0x00000003
#define CONSOLE_HANDLE_NEVERSET  0x10000000
#define CONSOLE_HANDLE_MASK      (CONSOLE_HANDLE_SIGNATURE | CONSOLE_HANDLE_NEVERSET)

#define CONSOLE_HANDLE(HANDLE) (((ULONG_PTR)(HANDLE) & CONSOLE_HANDLE_MASK) == CONSOLE_HANDLE_SIGNATURE)

//
// These strings are used to open console input or output.
//

#define CONSOLE_INPUT_STRING  L"CONIN$"
#define CONSOLE_OUTPUT_STRING L"CONOUT$"
#define CONSOLE_GENERIC       L"CON"

//
// this string is used to call RegisterWindowMessage to get
// progman's handle.
//

#define CONSOLE_PROGMAN_HANDLE_MESSAGE "ConsoleProgmanHandle"


//
// stream API definitions.  these API are only supposed to be used by
// subsystems (i.e. OpenFile routes to OpenConsoleW).
//

HANDLE
APIENTRY
OpenConsoleW(
    _In_ LPWSTR lpConsoleDevice,
    _In_ DWORD dwDesiredAccess,
    _In_ BOOL bInheritHandle,
    _In_ DWORD dwShareMode);

HANDLE
APIENTRY
DuplicateConsoleHandle(
    _In_ HANDLE hSourceHandle,
    _In_ DWORD dwDesiredAccess,
    _In_ BOOL bInheritHandle,
    _In_ DWORD dwOptions);

BOOL
APIENTRY
GetConsoleHandleInformation(
    _In_ HANDLE hObject,
    _Out_ LPDWORD lpdwFlags);

BOOL
APIENTRY
SetConsoleHandleInformation(
    _In_ HANDLE hObject,
    _In_ DWORD dwMask,
    _In_ DWORD dwFlags);

BOOL
APIENTRY
CloseConsoleHandle(
    _In_ HANDLE hConsole);

BOOL
APIENTRY
VerifyConsoleIoHandle(
    _In_ HANDLE hIoHandle);

HANDLE
APIENTRY
GetConsoleInputWaitHandle(
    VOID);

typedef struct _CONSOLE_STATE_INFO {
    /* BEGIN V1 CONSOLE_STATE_INFO */
    COORD ScreenBufferSize;
    COORD WindowSize;
    INT WindowPosX;
    INT WindowPosY;
    COORD FontSize;
    UINT FontFamily;
    UINT FontWeight;
    WCHAR FaceName[LF_FACESIZE];
    UINT CursorSize;
    UINT FullScreen : 1;
    UINT QuickEdit : 1;
    UINT AutoPosition : 1;
    UINT InsertMode : 1;
    UINT HistoryNoDup : 1;
    UINT FullScreenSupported : 1;
    UINT UpdateValues : 1;
    UINT Defaults : 1;
    WORD ScreenAttributes;
    WORD PopupAttributes;
    UINT HistoryBufferSize;
    UINT NumberOfHistoryBuffers;
    COLORREF ColorTable[16];
    HWND hWnd;
    HICON hIcon;
    LPWSTR OriginalTitle;
    LPWSTR LinkTitle;

    /*
     * Starting code page
     */
    UINT CodePage;

    /* END V1 CONSOLE_STATE_INFO */

    /* BEGIN V2 CONSOLE_STATE_INFO */
    BOOL fIsV2Console;
    BOOL fWrapText;
    BOOL fFilterOnPaste;
    BOOL fCtrlKeyShortcutsDisabled;
    BOOL fLineSelection;
    BYTE bWindowTransparency;
    BOOL fWindowMaximized;

    unsigned int CursorType;
    COLORREF CursorColor;

    BOOL InterceptCopyPaste;

    COLORREF DefaultForeground;
    COLORREF DefaultBackground;
    BOOL TerminalScrolling;
    /* END V2 CONSOLE_STATE_INFO */

} CONSOLE_STATE_INFO, *PCONSOLE_STATE_INFO;


#ifdef DEFINE_CONSOLEV2_PROPERTIES
#define PID_CONSOLE_FORCEV2                 1
#define PID_CONSOLE_WRAPTEXT                2
#define PID_CONSOLE_FILTERONPASTE           3
#define PID_CONSOLE_CTRLKEYSDISABLED        4
#define PID_CONSOLE_LINESELECTION           5
#define PID_CONSOLE_WINDOWTRANSPARENCY      6
#define PID_CONSOLE_WINDOWMAXIMIZED         7
#define PID_CONSOLE_CURSOR_TYPE             8
#define PID_CONSOLE_CURSOR_COLOR            9
#define PID_CONSOLE_INTERCEPT_COPY_PASTE    10
#define PID_CONSOLE_DEFAULTFOREGROUND       11
#define PID_CONSOLE_DEFAULTBACKGROUND       12
#define PID_CONSOLE_TERMINALSCROLLING       13

#define CONSOLE_PROPKEY(name, id) \
DEFINE_PROPERTYKEY(name, 0x0C570607, 0x0396, 0x43DE, 0x9D, 0x61, 0xE3, 0x21, 0xD7, 0xDF, 0x50, 0x26, id);

CONSOLE_PROPKEY(PKEY_Console_ForceV2,                   PID_CONSOLE_FORCEV2);
CONSOLE_PROPKEY(PKEY_Console_WrapText,                  PID_CONSOLE_WRAPTEXT);
CONSOLE_PROPKEY(PKEY_Console_FilterOnPaste,             PID_CONSOLE_FILTERONPASTE);
CONSOLE_PROPKEY(PKEY_Console_CtrlKeyShortcutsDisabled,  PID_CONSOLE_CTRLKEYSDISABLED);
CONSOLE_PROPKEY(PKEY_Console_LineSelection,             PID_CONSOLE_LINESELECTION);
CONSOLE_PROPKEY(PKEY_Console_WindowTransparency,        PID_CONSOLE_WINDOWTRANSPARENCY);
CONSOLE_PROPKEY(PKEY_Console_WindowMaximized,           PID_CONSOLE_WINDOWMAXIMIZED);
CONSOLE_PROPKEY(PKEY_Console_CursorType,                PID_CONSOLE_CURSOR_TYPE);
CONSOLE_PROPKEY(PKEY_Console_CursorColor,               PID_CONSOLE_CURSOR_COLOR);
CONSOLE_PROPKEY(PKEY_Console_InterceptCopyPaste,        PID_CONSOLE_INTERCEPT_COPY_PASTE);
CONSOLE_PROPKEY(PKEY_Console_DefaultForeground,         PID_CONSOLE_DEFAULTFOREGROUND);
CONSOLE_PROPKEY(PKEY_Console_DefaultBackground,         PID_CONSOLE_DEFAULTBACKGROUND);
CONSOLE_PROPKEY(PKEY_Console_TerminalScrolling,         PID_CONSOLE_TERMINALSCROLLING);
#endif


//
// Ensure the alignment is WORD boundary
//

#include <pshpack2.h>

typedef struct {
    WORD wMod;
    WORD wVirKey;
    WCHAR wUnicodeChar;
} ExtKeySubst;

typedef struct {
    ExtKeySubst keys[3];    // 0: Ctrl
                            // 1: Alt
                            // 2: Ctrl+Alt
} ExtKeyDef;

typedef ExtKeyDef ExtKeyDefTable['Z' - 'A' + 1];

typedef struct {
    DWORD dwVersion;
    DWORD dwCheckSum;
    ExtKeyDefTable table;
} ExtKeyDefBuf;

//
// Restore the previous alignment
//

#include <poppack.h>


#if _MSC_VER >= 1200
#pragma warning(pop)
#endif

#ifdef __cplusplus
}
#endif

#endif // _WINCONP_
