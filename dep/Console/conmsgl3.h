/*++

Copyright (c) 1985 - 1999, Microsoft Corporation

Module Name:

    conmsgl3.h

Abstract:

    This include file defines the message formats used to communicate
    between the client and server portions of the CONSOLE portion of the
    Windows subsystem.

Author:

    Therese Stowell (thereses) 10-Nov-1990

Revision History:

    Wedson Almeida Filho (wedsonaf) 23-May-2010
        Modified the messages for use with the console driver.

--*/

#pragma once

#include <winconp.h>  // need FONT_SELECT

typedef struct _CONSOLE_GETNUMBEROFFONTS_MSG {
    OUT ULONG NumberOfFonts;
} CONSOLE_GETNUMBEROFFONTS_MSG, *PCONSOLE_GETNUMBEROFFONTS_MSG;

typedef struct _CONSOLE_GETSELECTIONINFO_MSG {
    OUT CONSOLE_SELECTION_INFO SelectionInfo;
} CONSOLE_GETSELECTIONINFO_MSG, *PCONSOLE_GETSELECTIONINFO_MSG;

typedef struct _CONSOLE_GETMOUSEINFO_MSG {
    OUT ULONG NumButtons;
} CONSOLE_GETMOUSEINFO_MSG, *PCONSOLE_GETMOUSEINFO_MSG;

typedef struct _CONSOLE_GETFONTINFO_MSG {
    IN BOOLEAN MaximumWindow;
    OUT ULONG NumFonts;  // this value is valid even for error cases
} CONSOLE_GETFONTINFO_MSG, *PCONSOLE_GETFONTINFO_MSG;

typedef struct _CONSOLE_GETFONTSIZE_MSG {
    IN ULONG  FontIndex;
    OUT COORD FontSize;
} CONSOLE_GETFONTSIZE_MSG, *PCONSOLE_GETFONTSIZE_MSG;

typedef struct _CONSOLE_CURRENTFONT_MSG {
    IN BOOLEAN MaximumWindow;
    IN OUT ULONG FontIndex;
    IN OUT COORD FontSize;
    IN OUT ULONG FontFamily;
    IN OUT ULONG FontWeight;
    IN OUT WCHAR FaceName[LF_FACESIZE];
} CONSOLE_CURRENTFONT_MSG, *PCONSOLE_CURRENTFONT_MSG;

typedef struct _CONSOLE_SETFONT_MSG {
    IN ULONG  FontIndex;
} CONSOLE_SETFONT_MSG, *PCONSOLE_SETFONT_MSG;

typedef struct _CONSOLE_SETICON_MSG {
    IN HICON hIcon;
} CONSOLE_SETICON_MSG, *PCONSOLE_SETICON_MSG;

typedef struct _CONSOLE_SETICON_MSG64 {
    IN PVOID64 hIcon;
} CONSOLE_SETICON_MSG64, *PCONSOLE_SETICON_MSG64;

typedef struct _CONSOLE_ADDALIAS_MSG {
    IN USHORT SourceLength;
    IN USHORT TargetLength;
    IN USHORT ExeLength;
    IN BOOLEAN Unicode;
} CONSOLE_ADDALIAS_MSG, *PCONSOLE_ADDALIAS_MSG;

typedef struct _CONSOLE_GETALIAS_MSG {
    IN USHORT SourceLength;
    OUT USHORT TargetLength;
    IN USHORT ExeLength;
    IN BOOLEAN Unicode;
} CONSOLE_GETALIAS_MSG, *PCONSOLE_GETALIAS_MSG;

typedef struct _CONSOLE_GETALIASESLENGTH_MSG {
    OUT ULONG AliasesLength;
    IN BOOLEAN Unicode;
} CONSOLE_GETALIASESLENGTH_MSG, *PCONSOLE_GETALIASESLENGTH_MSG;

typedef struct _CONSOLE_GETALIASEXESLENGTH_MSG {
    OUT ULONG AliasExesLength;
    IN BOOLEAN Unicode;
} CONSOLE_GETALIASEXESLENGTH_MSG, *PCONSOLE_GETALIASEXESLENGTH_MSG;

typedef struct _CONSOLE_GETALIASES_MSG {
    IN BOOLEAN Unicode;
    OUT ULONG AliasesBufferLength;
} CONSOLE_GETALIASES_MSG, *PCONSOLE_GETALIASES_MSG;

typedef struct _CONSOLE_GETALIASEXES_MSG {
    OUT ULONG AliasExesBufferLength;
    IN BOOLEAN Unicode;
} CONSOLE_GETALIASEXES_MSG, *PCONSOLE_GETALIASEXES_MSG;

typedef struct _CONSOLE_EXPUNGECOMMANDHISTORY_MSG {
    IN BOOLEAN Unicode;
} CONSOLE_EXPUNGECOMMANDHISTORY_MSG, *PCONSOLE_EXPUNGECOMMANDHISTORY_MSG;

typedef struct _CONSOLE_SETNUMBEROFCOMMANDS_MSG {
    IN ULONG NumCommands;
    IN BOOLEAN Unicode;
} CONSOLE_SETNUMBEROFCOMMANDS_MSG, *PCONSOLE_SETNUMBEROFCOMMANDS_MSG;

typedef struct _CONSOLE_GETCOMMANDHISTORYLENGTH_MSG {
    OUT ULONG CommandHistoryLength;
    IN BOOLEAN Unicode;
} CONSOLE_GETCOMMANDHISTORYLENGTH_MSG, *PCONSOLE_GETCOMMANDHISTORYLENGTH_MSG;

typedef struct _CONSOLE_GETCOMMANDHISTORY_MSG {
    OUT ULONG CommandBufferLength;
    IN BOOLEAN Unicode;
} CONSOLE_GETCOMMANDHISTORY_MSG, *PCONSOLE_GETCOMMANDHISTORY_MSG;

typedef struct _CONSOLE_INVALIDATERECT_MSG {
    IN SMALL_RECT Rect;
} CONSOLE_INVALIDATERECT_MSG, *PCONSOLE_INVALIDATERECT_MSG;

typedef struct _CONSOLE_VDM_MSG {
    IN ULONG iFunction;
    OUT BOOLEAN Bool;
    IN OUT POINT Point;
    OUT RECT Rect;
} CONSOLE_VDM_MSG, *PCONSOLE_VDM_MSG;

typedef struct _CONSOLE_SETCURSOR_MSG {
    IN HCURSOR CursorHandle;
} CONSOLE_SETCURSOR_MSG, *PCONSOLE_SETCURSOR_MSG;

typedef struct _CONSOLE_SETCURSOR_MSG64 {
    IN PVOID64 CursorHandle;
} CONSOLE_SETCURSOR_MSG64, *PCONSOLE_SETCURSOR_MSG64;

typedef struct _CONSOLE_SHOWCURSOR_MSG {
    IN BOOLEAN bShow;
    OUT ULONG DisplayCount;
} CONSOLE_SHOWCURSOR_MSG, *PCONSOLE_SHOWCURSOR_MSG;

typedef struct _CONSOLE_MENUCONTROL_MSG {
    IN ULONG CommandIdLow;
    IN ULONG CommandIdHigh;
    OUT HMENU hMenu;
} CONSOLE_MENUCONTROL_MSG, *PCONSOLE_MENUCONTROL_MSG;

typedef struct _CONSOLE_MENUCONTROL_MSG64 {
    IN ULONG CommandIdLow;
    IN ULONG CommandIdHigh;
    OUT PVOID64 hMenu;
} CONSOLE_MENUCONTROL_MSG64, *PCONSOLE_MENUCONTROL_MSG64;

typedef struct _CONSOLE_SETPALETTE_MSG {
    IN HPALETTE hPalette;
    IN ULONG dwUsage;
} CONSOLE_SETPALETTE_MSG, *PCONSOLE_SETPALETTE_MSG;

typedef struct _CONSOLE_SETPALETTE_MSG64 {
    IN PVOID64 hPalette;
    IN ULONG dwUsage;
} CONSOLE_SETPALETTE_MSG64, *PCONSOLE_SETPALETTE_MSG64;

typedef struct _CONSOLE_SETDISPLAYMODE_MSG {
    IN ULONG dwFlags;
    OUT COORD ScreenBufferDimensions;
} CONSOLE_SETDISPLAYMODE_MSG, *PCONSOLE_SETDISPLAYMODE_MSG;

typedef struct _CONSOLE_REGISTERVDM_MSG {
    IN ULONG RegisterFlags;
    IN HANDLE StartEvent;
    IN HANDLE EndEvent;
    IN HANDLE ErrorEvent;
    OUT ULONG StateLength;
    OUT PVOID StateBuffer;
    OUT PVOID VDMBuffer;
} CONSOLE_REGISTERVDM_MSG, *PCONSOLE_REGISTERVDM_MSG;

typedef struct _CONSOLE_REGISTERVDM_MSG64 {
    IN ULONG RegisterFlags;
    IN PVOID64 StartEvent;
    IN PVOID64 EndEvent;
    IN PVOID64 ErrorEvent;
    OUT ULONG StateLength;
    OUT PVOID64 StateBuffer;
    OUT PVOID64 VDMBuffer;
} CONSOLE_REGISTERVDM_MSG64, *PCONSOLE_REGISTERVDM_MSG64;

typedef struct _CONSOLE_GETHARDWARESTATE_MSG {
    OUT COORD Resolution;
    OUT COORD FontSize;
} CONSOLE_GETHARDWARESTATE_MSG, *PCONSOLE_GETHARDWARESTATE_MSG;

typedef struct _CONSOLE_SETHARDWARESTATE_MSG {
    IN COORD Resolution;
    IN COORD FontSize;
} CONSOLE_SETHARDWARESTATE_MSG, *PCONSOLE_SETHARDWARESTATE_MSG;

typedef struct _CONSOLE_GETDISPLAYMODE_MSG {
    OUT ULONG ModeFlags;
} CONSOLE_GETDISPLAYMODE_MSG, *PCONSOLE_GETDISPLAYMODE_MSG;

typedef struct _CONSOLE_GETKEYBOARDLAYOUTNAME_MSG {
    union {
        WCHAR awchLayout[9];
        char achLayout[9];
    };
    BOOLEAN bAnsi;
} CONSOLE_GETKEYBOARDLAYOUTNAME_MSG, *PCONSOLE_GETKEYBOARDLAYOUTNAME_MSG;

typedef struct _CONSOLE_SETKEYSHORTCUTS_MSG {
    IN BOOLEAN Set;
    IN BYTE ReserveKeys;
} CONSOLE_SETKEYSHORTCUTS_MSG, *PCONSOLE_SETKEYSHORTCUTS_MSG;

typedef struct _CONSOLE_SETMENUCLOSE_MSG {
    IN BOOLEAN Enable;
} CONSOLE_SETMENUCLOSE_MSG, *PCONSOLE_SETMENUCLOSE_MSG;

typedef struct _CONSOLE_CHAR_TYPE_MSG {
    IN COORD coordCheck;
    OUT ULONG dwType;
} CONSOLE_CHAR_TYPE_MSG, *PCONSOLE_CHAR_TYPE_MSG;

typedef struct _CONSOLE_LOCAL_EUDC_MSG {
    IN USHORT CodePoint;
    IN COORD FontSize;
} CONSOLE_LOCAL_EUDC_MSG, *PCONSOLE_LOCAL_EUDC_MSG;

typedef struct _CONSOLE_CURSOR_MODE_MSG {
    IN OUT BOOLEAN Blink;
    IN OUT BOOLEAN DBEnable;
} CONSOLE_CURSOR_MODE_MSG, *PCONSOLE_CURSOR_MODE_MSG;

typedef struct _CONSOLE_REGISTEROS2_MSG {
    IN BOOLEAN fOs2Register;
} CONSOLE_REGISTEROS2_MSG, *PCONSOLE_REGISTEROS2_MSG;

typedef struct _CONSOLE_SETOS2OEMFORMAT_MSG {
    IN BOOLEAN fOs2OemFormat;
} CONSOLE_SETOS2OEMFORMAT_MSG, *PCONSOLE_SETOS2OEMFORMAT_MSG;

typedef struct _CONSOLE_NLS_MODE_MSG {
    IN OUT BOOLEAN Ready;
    IN ULONG NlsMode;
} CONSOLE_NLS_MODE_MSG, *PCONSOLE_NLS_MODE_MSG;

typedef struct _CONSOLE_GETCONSOLEWINDOW_MSG {
    OUT HWND hwnd;
} CONSOLE_GETCONSOLEWINDOW_MSG, *PCONSOLE_GETCONSOLEWINDOW_MSG;

typedef struct _CONSOLE_GETCONSOLEWINDOW_MSG64 {
    OUT PVOID64 hwnd;
} CONSOLE_GETCONSOLEWINDOW_MSG64, *PCONSOLE_GETCONSOLEWINDOW_MSG64;

typedef struct _CONSOLE_GETPROCESSLIST_MSG {
    OUT ULONG dwProcessCount;
} CONSOLE_GETCONSOLEPROCESSLIST_MSG, *PCONSOLE_GETCONSOLEPROCESSLIST_MSG;

typedef struct _CONSOLE_GETHISTORY_MSG {
    OUT ULONG HistoryBufferSize;
    OUT ULONG NumberOfHistoryBuffers;
    OUT ULONG dwFlags;
} CONSOLE_HISTORY_MSG, *PCONSOLE_HISTORY_MSG;

typedef enum _CONSOLE_API_NUMBER_L3 {
    ConsolepGetNumberOfFonts = CONSOLE_FIRST_API_NUMBER(3),
    ConsolepGetMouseInfo,
    ConsolepGetFontInfo,
    ConsolepGetFontSize,
    ConsolepGetCurrentFont,
    ConsolepSetFont,
    ConsolepSetIcon,
    ConsolepInvalidateBitmapRect,
    ConsolepVDMOperation,
    ConsolepSetCursor,
    ConsolepShowCursor,
    ConsolepMenuControl,
    ConsolepSetPalette,
    ConsolepSetDisplayMode,
    ConsolepRegisterVDM,
    ConsolepGetHardwareState,
    ConsolepSetHardwareState,
    ConsolepGetDisplayMode,
    ConsolepAddAlias,
    ConsolepGetAlias,
    ConsolepGetAliasesLength,
    ConsolepGetAliasExesLength,
    ConsolepGetAliases,
    ConsolepGetAliasExes,
    ConsolepExpungeCommandHistory,
    ConsolepSetNumberOfCommands,
    ConsolepGetCommandHistoryLength,
    ConsolepGetCommandHistory,
    ConsolepSetKeyShortcuts,
    ConsolepSetMenuClose,
    ConsolepGetKeyboardLayoutName,
    ConsolepGetConsoleWindow,
    ConsolepCharType,
    ConsolepSetLocalEUDC,
    ConsolepSetCursorMode,
    ConsolepGetCursorMode,
    ConsolepRegisterOS2,
    ConsolepSetOS2OemFormat,
    ConsolepGetNlsMode,
    ConsolepSetNlsMode,
    ConsolepGetSelectionInfo,
    ConsolepGetConsoleProcessList,
    ConsolepGetHistory,
    ConsolepSetHistory,
    ConsolepSetCurrentFont,
} CONSOLE_API_NUMBER_L3, *PCONSOLE_API_NUMBER_L3;

typedef union _CONSOLE_MSG_BODY_L3 {
    CONSOLE_GETNUMBEROFFONTS_MSG GetNumberOfConsoleFonts;
    CONSOLE_GETMOUSEINFO_MSG GetConsoleMouseInfo;
    CONSOLE_GETFONTINFO_MSG GetConsoleFontInfo;
    CONSOLE_GETFONTSIZE_MSG GetConsoleFontSize;
    CONSOLE_CURRENTFONT_MSG GetCurrentConsoleFont;
    CONSOLE_SETFONT_MSG SetConsoleFont;
    CONSOLE_INVALIDATERECT_MSG InvalidateConsoleBitmapRect;
    CONSOLE_VDM_MSG VDMConsoleOperation;
    CONSOLE_SHOWCURSOR_MSG ShowConsoleCursor;
    CONSOLE_SETDISPLAYMODE_MSG SetConsoleDisplayMode;
#ifdef BUILD_WOW6432
    CONSOLE_REGISTERVDM_MSG64 RegisterConsoleVDM;
    CONSOLE_SETCURSOR_MSG64 SetConsoleCursor;
    CONSOLE_SETICON_MSG64 SetConsoleIcon;
    CONSOLE_MENUCONTROL_MSG64 ConsoleMenuControl;
    CONSOLE_SETPALETTE_MSG64 SetConsolePalette;
    CONSOLE_GETCONSOLEWINDOW_MSG64 GetConsoleWindow;
#else
    CONSOLE_REGISTERVDM_MSG RegisterConsoleVDM;
    CONSOLE_SETCURSOR_MSG SetConsoleCursor;
    CONSOLE_SETICON_MSG SetConsoleIcon;
    CONSOLE_MENUCONTROL_MSG ConsoleMenuControl;
    CONSOLE_SETPALETTE_MSG SetConsolePalette;
    CONSOLE_GETCONSOLEWINDOW_MSG GetConsoleWindow;
#endif
    CONSOLE_GETHARDWARESTATE_MSG GetConsoleHardwareState;
    CONSOLE_SETHARDWARESTATE_MSG SetConsoleHardwareState;
    CONSOLE_GETDISPLAYMODE_MSG GetConsoleDisplayMode;
    CONSOLE_ADDALIAS_MSG AddConsoleAliasW;
    CONSOLE_GETALIAS_MSG GetConsoleAliasW;
    CONSOLE_GETALIASESLENGTH_MSG GetConsoleAliasesLengthW;
    CONSOLE_GETALIASEXESLENGTH_MSG GetConsoleAliasExesLengthW;
    CONSOLE_GETALIASES_MSG GetConsoleAliasesW;
    CONSOLE_GETALIASEXES_MSG GetConsoleAliasExesW;
    CONSOLE_EXPUNGECOMMANDHISTORY_MSG ExpungeConsoleCommandHistoryW;
    CONSOLE_SETNUMBEROFCOMMANDS_MSG SetConsoleNumberOfCommandsW;
    CONSOLE_GETCOMMANDHISTORYLENGTH_MSG GetConsoleCommandHistoryLengthW;
    CONSOLE_GETCOMMANDHISTORY_MSG GetConsoleCommandHistoryW;
    CONSOLE_SETKEYSHORTCUTS_MSG SetConsoleKeyShortcuts;
    CONSOLE_SETMENUCLOSE_MSG SetConsoleMenuClose;
    CONSOLE_GETKEYBOARDLAYOUTNAME_MSG GetKeyboardLayoutName;
    CONSOLE_CHAR_TYPE_MSG GetConsoleCharType;
    CONSOLE_LOCAL_EUDC_MSG SetConsoleLocalEUDC;
    CONSOLE_CURSOR_MODE_MSG SetConsoleCursorMode;
    CONSOLE_CURSOR_MODE_MSG GetConsoleCursorMode;
    CONSOLE_REGISTEROS2_MSG RegisterConsoleOS2;
    CONSOLE_SETOS2OEMFORMAT_MSG SetConsoleOS2OemFormat;
    CONSOLE_NLS_MODE_MSG GetConsoleNlsMode;
    CONSOLE_NLS_MODE_MSG SetConsoleNlsMode;
    CONSOLE_GETSELECTIONINFO_MSG GetConsoleSelectionInfo;
    CONSOLE_GETCONSOLEPROCESSLIST_MSG GetConsoleProcessList;
    CONSOLE_CURRENTFONT_MSG SetCurrentConsoleFont;
    CONSOLE_HISTORY_MSG SetConsoleHistory;
    CONSOLE_HISTORY_MSG GetConsoleHistory;
} CONSOLE_MSG_BODY_L3, *PCONSOLE_MSG_BODY_L3;

#ifndef __cplusplus
typedef struct _CONSOLE_MSG_L3 {
    CONSOLE_MSG_HEADER Header;
    union {
        CONSOLE_MSG_BODY_L3;
    } u;
} CONSOLE_MSG_L3, *PCONSOLE_MSG_L3;
#else
typedef struct _CONSOLE_MSG_L3 :
    public CONSOLE_MSG_HEADER
{
    CONSOLE_MSG_BODY_L3 u;
} CONSOLE_MSG_L3, *PCONSOLE_MSG_L3;
#endif  // __cplusplus
