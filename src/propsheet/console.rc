/****************************** Module Header ******************************\
* Module Name: console.rc
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* Constants
*
* History:
* 08-21-91      Created.
\***************************************************************************/


#include <windows.h>

#ifndef EXTERNAL_BUILD
#include <ntverp.h>
#endif
#include <commctrl.h>
#include "dialogs.h"
#include "console.h"

IDI_CONSOLE      ICON   "..\\..\\res\\console.ico"

#include "strid.rc"

//
//    Dialogs
//


//
// This is the template for the defaults settings dialog for use with ComCtlv6
//

DID_SETTINGS DIALOG  0, 0, 240, 240
CAPTION " Options "
STYLE WS_VISIBLE | WS_CAPTION | WS_CHILD | DS_MODALFRAME
FONT 8,"MS Shell Dlg"
BEGIN
    GROUPBOX        "Cursor Size", -1, 10, 11, 100, 56
    AUTORADIOBUTTON "&Small", IDD_CURSOR_SMALL, 14, 23, 84, 10,
                    WS_TABSTOP | WS_GROUP
    AUTORADIOBUTTON "&Medium", IDD_CURSOR_MEDIUM, 14, 33, 84, 10,
    AUTORADIOBUTTON "&Large", IDD_CURSOR_LARGE, 14, 43, 84, 10,
    // IDD_CURSOR_ADVANCED is a hidden control, see GH#1219
    AUTORADIOBUTTON "", IDD_CURSOR_ADVANCED, 14, 53, 0, 0,

    GROUPBOX        "Command History", -1, 115, 11, 120, 56, WS_GROUP
    LTEXT           "&Buffer Size:", -1, 119, 25, 78, 9
    EDITTEXT        IDD_HISTORY_SIZE, 197, 23, 31, 12, WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_HISTORY_SIZESCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    LTEXT           "&Number of Buffers:", -1, 119, 40, 78, 9
    EDITTEXT        IDD_HISTORY_NUM, 197, 38, 31, 12, WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_HISTORY_NUMSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    AUTOCHECKBOX    "&Discard Old Duplicates", IDD_HISTORY_NODUP, 119,55, 108, 9

    GROUPBOX        "Edit Options",-1,10,70,225,64
    CONTROL         "&QuickEdit Mode", IDD_QUICKEDIT, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 14, 82, 75, 10
    CONTROL         "&Insert Mode", IDD_INSERT, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 14, 92, 75, 10
    CONTROL         "Enable Ctrl &key shortcuts", IDD_CTRL_KEYS_ENABLED, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 102, 200, 10
    CONTROL         "&Filter clipboard contents on paste", IDD_FILTER_ON_PASTE, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 112, 200, 10
    CONTROL         "Use Ctrl+Shift+C/V as &Copy/Paste", IDD_INTERCEPT_COPY_PASTE, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 122, 200, 10

    GROUPBOX        "Text Selection",-1,10,136,225,32
    CONTROL         "&Enable line wrapping selection", IDD_LINE_SELECTION, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 146, 200, 10
    CONTROL         "E&xtended text selection keys", IDD_EDIT_KEYS, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 156, 200, 10

    GROUPBOX        "Default code page", IDD_LANGUAGE_GROUPBOX,   10, 160, 225, 29, WS_GROUP
    COMBOBOX        IDD_LANGUAGELIST, 16, 171, 213, 78,
                    CBS_SORT |
                    CBS_DISABLENOSCROLL |
                    CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP | WS_GROUP

    CONTROL         "&Use legacy console (requires relaunch, affects all consoles)", IDD_FORCEV2, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 10, 199, 200, 10

    CONTROL         "Learn more about <A HREF=""https://go.microsoft.com/fwlink/?LinkId=871150"">legacy console mode</A>",
                    IDD_HELP_LEGACY_LINK, "SysLink", WS_TABSTOP, 21, 211, 179, 10

    CONTROL         "Find out more about <A HREF=""https://go.microsoft.com/fwlink/?LinkId=507549"">new console features</A>",
                    IDD_HELP_SYSLINK, "SysLink", WS_TABSTOP, 10, 225, 200, 10


END

//
// This is the template for the specifics settings dialog for use with ComCtlv6
//

DID_SETTINGS2 DIALOG  0, 0, 245, 240
CAPTION " Options "
STYLE WS_VISIBLE | WS_CAPTION | WS_CHILD | DS_MODALFRAME
FONT 8,"MS Shell Dlg"
BEGIN
    GROUPBOX        "Cursor Size", -1, 10, 11, 100, 56
    AUTORADIOBUTTON "&Small", IDD_CURSOR_SMALL, 14, 23, 84, 10,
                    WS_TABSTOP | WS_GROUP
    AUTORADIOBUTTON "&Medium", IDD_CURSOR_MEDIUM, 14, 33, 84, 10,
    AUTORADIOBUTTON "&Large", IDD_CURSOR_LARGE, 14, 43, 84, 10,
    // IDD_CURSOR_ADVANCED is a hidden control, see GH#1219
    AUTORADIOBUTTON "", IDD_CURSOR_ADVANCED, 14, 53, 0, 0,

    GROUPBOX        "Command History", -1, 115, 11, 120, 56, WS_GROUP
    LTEXT           "&Buffer Size:", -1, 119, 25, 78, 9
    EDITTEXT        IDD_HISTORY_SIZE, 197, 23, 31, 12, WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_HISTORY_SIZESCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    LTEXT           "&Number of Buffers:", -1, 119, 40, 78, 9
    EDITTEXT        IDD_HISTORY_NUM, 197, 38, 31, 12, WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_HISTORY_NUMSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    AUTOCHECKBOX    "&Discard Old Duplicates", IDD_HISTORY_NODUP, 119,55, 108, 9

    GROUPBOX        "Edit Options",-1,10,70,225,64
    CONTROL         "&QuickEdit Mode", IDD_QUICKEDIT, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 14, 82, 75, 10
    CONTROL         "&Insert Mode", IDD_INSERT, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 14, 92, 75, 10
    CONTROL         "Enable Ctrl &key shortcuts", IDD_CTRL_KEYS_ENABLED, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 102, 200, 10
    CONTROL         "&Filter clipboard contents on paste", IDD_FILTER_ON_PASTE, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 112, 200, 10
    CONTROL         "Use Ctrl+Shift+C/V as &Copy/Paste", IDD_INTERCEPT_COPY_PASTE, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 122, 200, 10

    GROUPBOX        "Text Selection",-1,10,136,225,32
    CONTROL         "&Enable line wrapping selection", IDD_LINE_SELECTION, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 146, 200, 10
    CONTROL         "E&xtended text selection keys", IDD_EDIT_KEYS, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 156, 200, 10

    GROUPBOX        "Current code page", IDD_LANGUAGE_GROUPBOX, 10, 170, 225, 24, WS_GROUP
    LTEXT           "",IDD_LANGUAGE, 16, 181, 184, 10, WS_GROUP

    CONTROL         "&Use legacy console (requires relaunch, affects all consoles)", IDD_FORCEV2, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 10, 199, 200, 10

    CONTROL         "Learn more about <A HREF=""https://go.microsoft.com/fwlink/?LinkId=871150"">legacy console mode</A>",
                    IDD_HELP_LEGACY_LINK, "SysLink", WS_TABSTOP, 21, 211, 179, 10

    CONTROL         "Find out more about <A HREF=""https://go.microsoft.com/fwlink/?LinkId=507549"">new console features</A>",
                    IDD_HELP_SYSLINK, "SysLink", WS_TABSTOP, 10, 225, 200, 10

END

//
// This is the template for the defaults settings dialog for use with ComCtlv5
// At the time of writing, this only differed from the above by the Hyperlink control (which is only in v6)
//

DID_SETTINGS_COMCTL5 DIALOG  0, 0, 240, 226
CAPTION " Options "
STYLE WS_VISIBLE | WS_CAPTION | WS_CHILD | DS_MODALFRAME
FONT 8,"MS Shell Dlg"
BEGIN
    GROUPBOX        "Cursor Size", -1, 10, 11, 100, 56
    AUTORADIOBUTTON "&Small", IDD_CURSOR_SMALL, 14, 23, 84, 10,
                    WS_TABSTOP | WS_GROUP
    AUTORADIOBUTTON "&Medium", IDD_CURSOR_MEDIUM, 14, 33, 84, 10,
    AUTORADIOBUTTON "&Large", IDD_CURSOR_LARGE, 14, 43, 84, 10,

    GROUPBOX        "Command History", -1, 115, 11, 120, 56, WS_GROUP
    LTEXT           "&Buffer Size:", -1, 119, 25, 78, 9
    EDITTEXT        IDD_HISTORY_SIZE, 197, 23, 31, 12, WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_HISTORY_SIZESCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    LTEXT           "&Number of Buffers:", -1, 119, 40, 78, 9
    EDITTEXT        IDD_HISTORY_NUM, 197, 38, 31, 12, WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_HISTORY_NUMSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    AUTOCHECKBOX    "&Discard Old Duplicates", IDD_HISTORY_NODUP, 119,55, 108, 9

    GROUPBOX        "Edit Options",-1,10,70,225,54
    CONTROL         "&QuickEdit Mode", IDD_QUICKEDIT, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 14, 82, 75, 10
    CONTROL         "&Insert Mode", IDD_INSERT, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 14, 92, 75, 10
    CONTROL         "Enable Ctrl &key shortcuts", IDD_CTRL_KEYS_ENABLED, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 102, 200, 10
    CONTROL         "&Filter clipboard contents on paste", IDD_FILTER_ON_PASTE, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 112, 200, 10

    GROUPBOX        "Text Selection",-1,10,126,225,32
    CONTROL         "&Enable line wrapping selection", IDD_LINE_SELECTION, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 136, 200, 10
    CONTROL         "E&xtended text selection keys", IDD_EDIT_KEYS, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 146, 200, 10

    GROUPBOX        "Default code page", IDD_LANGUAGE_GROUPBOX,   10, 160, 225, 29, WS_GROUP
    COMBOBOX        IDD_LANGUAGELIST, 16, 171, 213, 78,
                    CBS_SORT |
                    CBS_DISABLENOSCROLL |
                    CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP | WS_GROUP

    CONTROL         "&Use legacy console (requires relaunch, affects all consoles)", IDD_FORCEV2, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 10, 194, 200, 10

END

//
// This is the template for the specifics settings dialog for use with ComCtlv5
// At the time of writing, this only differed from the above by the Hyperlink control (which is only in v6)
//

DID_SETTINGS2_COMCTL5 DIALOG  0, 0, 245, 226
CAPTION " Options "
STYLE WS_VISIBLE | WS_CAPTION | WS_CHILD | DS_MODALFRAME
FONT 8,"MS Shell Dlg"
BEGIN
    GROUPBOX        "Cursor Size", -1, 10, 11, 100, 56
    AUTORADIOBUTTON "&Small", IDD_CURSOR_SMALL, 14, 23, 84, 10,
                    WS_TABSTOP | WS_GROUP
    AUTORADIOBUTTON "&Medium", IDD_CURSOR_MEDIUM, 14, 33, 84, 10,
    AUTORADIOBUTTON "&Large", IDD_CURSOR_LARGE, 14, 43, 84, 10,

    GROUPBOX        "Command History", -1, 115, 11, 120, 56, WS_GROUP
    LTEXT           "&Buffer Size:", -1, 119, 25, 78, 9
    EDITTEXT        IDD_HISTORY_SIZE, 197, 23, 31, 12, WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_HISTORY_SIZESCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    LTEXT           "&Number of Buffers:", -1, 119, 40, 78, 9
    EDITTEXT        IDD_HISTORY_NUM, 197, 38, 31, 12, WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_HISTORY_NUMSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    AUTOCHECKBOX    "&Discard Old Duplicates", IDD_HISTORY_NODUP, 119,55, 108, 9

    GROUPBOX        "Edit Options",-1,10,70,225,54
    CONTROL         "&QuickEdit Mode", IDD_QUICKEDIT, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 14, 82, 75, 10
    CONTROL         "&Insert Mode", IDD_INSERT, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 14, 92, 75, 10
    CONTROL         "Enable Ctrl &key shortcuts", IDD_CTRL_KEYS_ENABLED, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 102, 200, 10
    CONTROL         "&Filter clipboard contents on paste", IDD_FILTER_ON_PASTE, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 112, 200, 10

    GROUPBOX        "Text Selection",-1,10,126,225,32
    CONTROL         "&Enable line wrapping selection", IDD_LINE_SELECTION, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 136, 200, 10
    CONTROL         "E&xtended text selection keys", IDD_EDIT_KEYS, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 146, 200, 10

    GROUPBOX        "Current code page", IDD_LANGUAGE_GROUPBOX, 10, 160, 225, 24, WS_GROUP
    LTEXT           "",IDD_LANGUAGE, 16, 171, 184, 10, WS_GROUP

    CONTROL         "&Use legacy console (requires relaunch, affects all consoles)", IDD_FORCEV2, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 10, 194, 200, 10
END

//
// This is the template for the font selection dialog
//

DID_FONTDLG DIALOG 0, 0, 233, 226
CAPTION " Font "
STYLE WS_VISIBLE | WS_CAPTION | WS_CHILD | DS_MODALFRAME
FONT 8,"MS Shell Dlg"
BEGIN
// PixelSize listbox & PointSize combobox (top left)
//
    GROUPBOX        "&Size", IDD_FONTSIZE, 5, 4, 110, 88
    LISTBOX         IDD_PIXELSLIST,                   12, 17,  50, 73,
                    LBS_DISABLENOSCROLL | WS_VSCROLL | WS_TABSTOP
    COMBOBOX        IDD_POINTSLIST,                   12, 17, 30, 76,
                    CBS_SORT |
                    CBS_DISABLENOSCROLL | WS_VSCROLL | WS_TABSTOP

// Window Preview (top right)
//
    LTEXT           "Window Preview", IDD_PREVIEWLABEL, 125,  4, 109, 8
    CONTROL         "", IDD_PREVIEWWINDOW, "WOAWinPreview",
                    WS_CHILD,  125, 17, 109, 83

// FaceName listbox (top middle)
    GROUPBOX        "&Font", IDD_STATIC,              5, 96, 228, 68
    LISTBOX         IDD_FACENAME,                     12, 108, 117, 42,
                    LBS_SORT | WS_VSCROLL | WS_TABSTOP |
                    LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS

    LTEXT           "TrueType fonts are recommended for high DPI displays as raster fonts may not display clearly.",
                    -1, 134, 107, 94, 49

// Bold Checkbox (bottom middle)
//
    CONTROL         "&Bold fonts", IDD_BOLDFONT, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 12, 151, 60, 10

// Bottom portion (middle)
//
    GROUPBOX        "", IDD_GROUP, 5, 170, 228, 50
    CONTROL         "", IDD_FONTWINDOW, "WOAFontPreview",
                    WS_CHILD, 12, 180, 117, 34
    LTEXT           "Each character is:", IDD_STATIC2, 135, 180, 75, 8, NOT
                    WS_GROUP
    RTEXT           "", IDD_FONTWIDTH, 134, 190, 12, 8, NOT WS_GROUP
    LTEXT           "screen pixels wide", IDD_STATIC3, 148, 190, 65, 8, NOT
                    WS_GROUP
    LTEXT           "screen pixels high", IDD_STATIC4, 148, 200, 65, 8, NOT
                    WS_GROUP
    RTEXT           "", IDD_FONTHEIGHT, 134, 200, 12, 8, NOT WS_GROUP

END

//
// This is the template for the screenbuffer size dialog
//

DID_SCRBUFSIZE DIALOG  0, 0, 233, 226
CAPTION " Layout "
STYLE WS_VISIBLE | WS_CAPTION | WS_CHILD | DS_MODALFRAME
FONT 8,"MS Shell Dlg"
BEGIN
// Window Preview (top left)
//
    LTEXT           "Window Preview", IDD_PREVIEWLABEL, 130,  11, 109, 8
    CONTROL         "", IDD_PREVIEWWINDOW, "WOAWinPreview",
                    WS_CHILD,  130, 21, 109, 83

    GROUPBOX        "Screen Buffer Size", -1, 10, 11, 110, 56
    LTEXT           "&Width:", -1, 14, 25, 54, 9
    EDITTEXT        IDD_SCRBUF_WIDTH, 78, 23, 36, 12,
                    ES_AUTOHSCROLL | WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_SCRBUF_WIDTHSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    LTEXT           "&Height:", -1, 14, 39, 54, 9
    EDITTEXT        IDD_SCRBUF_HEIGHT, 78, 37, 36, 12,
                    ES_AUTOHSCROLL | WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_SCRBUF_HEIGHTSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    CONTROL         "W&rap text output on resize", IDD_LINE_WRAP, "Button",
                    BS_AUTOCHECKBOX | WS_TABSTOP, 14, 52, 100, 12

    GROUPBOX        "Window Size", -1, 10, 69, 110, 42
    LTEXT           "W&idth:", -1, 14, 83, 54, 9
    EDITTEXT        IDD_WINDOW_WIDTH, 78, 81, 36, 12,
                    ES_AUTOHSCROLL | WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_WINDOW_WIDTHSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    LTEXT           "H&eight:", -1, 14, 97, 54, 9
    EDITTEXT        IDD_WINDOW_HEIGHT, 78, 95, 36, 12,
                    ES_AUTOHSCROLL | WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_WINDOW_HEIGHTSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0

    GROUPBOX        "Window Position", -1, 10, 113, 110, 56
    LTEXT           "&Left:", -1, 14, 127, 54, 9
    EDITTEXT        IDD_WINDOW_POSX, 78, 125, 36, 12,
                    ES_AUTOHSCROLL | WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_WINDOW_POSXSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    LTEXT           "&Top:", -1, 14, 141, 54, 9
    EDITTEXT        IDD_WINDOW_POSY, 78, 139, 36, 12,
                    ES_AUTOHSCROLL | WS_GROUP | WS_TABSTOP
    CONTROL         "", IDD_WINDOW_POSYSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    CONTROL         "Let system &position window", IDD_AUTO_POSITION, "Button",
                    BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 14, 156, 100, 10
END

//
// This is the template for the screen colors dialog
//

DID_COLOR DIALOG  0, 0, 233, 226
CAPTION " Colors "
STYLE WS_VISIBLE | WS_CAPTION | WS_CHILD | DS_MODALFRAME
FONT 8,"MS Shell Dlg"
BEGIN
    AUTORADIOBUTTON "Screen &Text", IDD_COLOR_SCREEN_TEXT, 10, 10, 104, 12,
                    WS_TABSTOP|WS_GROUP
    AUTORADIOBUTTON "Screen &Background", IDD_COLOR_SCREEN_BKGND, 10, 22, 104, 12,
    AUTORADIOBUTTON "&Popup Text", IDD_COLOR_POPUP_TEXT, 10, 34, 104, 12,
    AUTORADIOBUTTON "Pop&up Background", IDD_COLOR_POPUP_BKGND, 10, 46, 104, 12,

    CONTROL         "", IDD_COLOR_1, "ColorTableColor",
                    WS_BORDER | WS_CHILD | WS_GROUP | WS_TABSTOP,
                    12, 70, 13, 13
    CONTROL         "", IDD_COLOR_2, "ColorTableColor", WS_BORDER | WS_CHILD,
                    25, 70, 13, 13
    CONTROL         "", IDD_COLOR_3, "ColorTableColor", WS_BORDER | WS_CHILD,
                    38, 70, 13, 13
    CONTROL         "", IDD_COLOR_4, "ColorTableColor", WS_BORDER | WS_CHILD,
                    51, 70, 13, 13
    CONTROL         "", IDD_COLOR_5, "ColorTableColor", WS_BORDER | WS_CHILD,
                    64, 70, 13, 13
    CONTROL         "", IDD_COLOR_6, "ColorTableColor", WS_BORDER | WS_CHILD,
                    77, 70, 13, 13
    CONTROL         "", IDD_COLOR_7, "ColorTableColor", WS_BORDER | WS_CHILD,
                    90, 70, 13, 13
    CONTROL         "", IDD_COLOR_8, "ColorTableColor", WS_BORDER | WS_CHILD,
                    103, 70, 13, 13
    CONTROL         "", IDD_COLOR_9, "ColorTableColor", WS_BORDER | WS_CHILD,
                    116, 70, 13, 13
    CONTROL         "", IDD_COLOR_10, "ColorTableColor", WS_BORDER | WS_CHILD,
                    129, 70, 13, 13
    CONTROL         "", IDD_COLOR_11, "ColorTableColor", WS_BORDER | WS_CHILD,
                    142, 70, 13, 13
    CONTROL         "", IDD_COLOR_12, "ColorTableColor", WS_BORDER | WS_CHILD,
                    155, 70, 13, 13
    CONTROL         "", IDD_COLOR_13, "ColorTableColor", WS_BORDER | WS_CHILD,
                    168, 70, 13, 13
    CONTROL         "", IDD_COLOR_14, "ColorTableColor", WS_BORDER | WS_CHILD,
                    181, 70, 13, 13
    CONTROL         "", IDD_COLOR_15, "ColorTableColor", WS_BORDER | WS_CHILD,
                    194, 70, 13, 13
    CONTROL         "", IDD_COLOR_16, "ColorTableColor", WS_BORDER | WS_CHILD,
                    207, 70, 13, 13

    GROUPBOX        "Selected Screen Colors", -1, 10, 84, 213, 46
    CONTROL         "", IDD_COLOR_SCREEN_COLORS, "WOAFontPreview",
                    WS_GROUP | WS_CHILD, 15, 94, 204, 31

    GROUPBOX        "Selected Popup Colors", -1, 10, 134, 213, 46
    CONTROL         "", IDD_COLOR_POPUP_COLORS, "WOAFontPreview",
                    WS_GROUP | WS_CHILD, 15, 144, 204, 31

    GROUPBOX        "Selected Color Values", -1, 120, 9, 103, 56
    LTEXT           "&Red:", -1, 124, 23, 54, 9
    EDITTEXT        IDD_COLOR_RED, 167, 21, 30, 12, WS_TABSTOP | WS_GROUP |
                    ES_AUTOHSCROLL
    CONTROL         "", IDD_COLOR_REDSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    LTEXT           "&Green:", -1, 124, 37, 54, 9
    EDITTEXT        IDD_COLOR_GREEN, 167, 35, 30, 12, WS_GROUP | WS_TABSTOP |
                    ES_AUTOHSCROLL
    CONTROL         "", IDD_COLOR_GREENSCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0
    LTEXT           "B&lue:", -1, 124, 51, 54, 9
    EDITTEXT        IDD_COLOR_BLUE, 167, 49, 30, 12, WS_GROUP | WS_TABSTOP |
                    ES_AUTOHSCROLL
    CONTROL         "", IDD_COLOR_BLUESCROLL, UPDOWN_CLASS,
                    UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                    UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0

    GROUPBOX        "&Opacity", IDD_OPACITY_GROUPBOX, 10, 182, 213, 38
    LTEXT           "30%", IDD_OPACITY_LOW_LABEL, 15, 194, 16, 12
    CONTROL         "Opacity", IDD_TRANSPARENCY, "msctls_trackbar32",
                    TBS_NOTICKS | TBS_HORZ | WS_TABSTOP, 35, 192, 135, 16
    LTEXT           "100%", IDD_OPACITY_HIGH_LABEL, 174, 194, 18, 12
    LTEXT           "", IDD_OPACITY_VALUE, 200, 193, 18, 10, SS_CENTER | SS_SUNKEN
END


//
// This is the template for the terminal dialog
//
// These defines help keep it sane when you're placing components relative to other components
// padding
#define P_0 2
#define P_1 8
#define P_2 (P_1*2)
#define P_3 (P_1*3)
#define P_4 (P_1*4)

#define COLOR_SIZE 13
// default Colors group box
#define T_COLORS_X 10
#define T_COLORS_Y 10
#define T_COLORS_W (225)
#define T_COLORS_H 65
#define T_COLORS_CHECK_Y (T_COLORS_Y+P_4)
#define T_COLORS_TEXT_W 32
#define T_COLORS_EDIT_W 30
#define T_COLORS_EDIT_H 12
#define T_COLORS_FG_X (T_COLORS_X+P_4)
#define T_COLORS_FG_W (100)
#define T_COLORS_RED_Y (T_COLORS_CHECK_Y+10)
#define T_COLORS_GREEN_Y (T_COLORS_RED_Y+T_COLORS_EDIT_H+P_0)
#define T_COLORS_BLUE_Y (T_COLORS_GREEN_Y+T_COLORS_EDIT_H+P_0)
#define T_COLORS_FG_TEXT_X (T_COLORS_X+P_4+P_1+COLOR_SIZE)
#define T_COLORS_FG_INPUT_X (T_COLORS_FG_TEXT_X+T_COLORS_TEXT_W)
#define T_COLORS_BG_X (T_COLORS_FG_X+T_COLORS_FG_W+P_1)
#define T_COLORS_BG_TEXT_X (T_COLORS_BG_X+P_1+COLOR_SIZE)
#define T_COLORS_BG_INPUT_X (T_COLORS_BG_TEXT_X+T_COLORS_TEXT_W)

// cursor styles group box
#define T_CSTYLE_X T_COLORS_X
#define T_CSTYLE_Y (T_COLORS_Y+T_COLORS_H+P_1)
#define T_CSTYLE_W 100
#define T_CSTYLE_H 75
// radio button dimensions
#define T_CSTYLE_R_W (T_CSTYLE_W-P_4-P_4)
#define T_CSTYLE_R_H (10)
// radio button positions
#define T_CSTYLE_R_1_Y (T_CSTYLE_Y+P_2)
#define T_CSTYLE_R_2_Y (T_CSTYLE_R_1_Y+T_CSTYLE_R_H)
#define T_CSTYLE_R_3_Y (T_CSTYLE_R_2_Y+T_CSTYLE_R_H)
#define T_CSTYLE_R_4_Y (T_CSTYLE_R_3_Y+T_CSTYLE_R_H)
#define T_CSTYLE_R_5_Y (T_CSTYLE_R_4_Y+T_CSTYLE_R_H)

#define T_CCOLOR_X (T_CSTYLE_X+T_CSTYLE_W+P_1)
#define T_CCOLOR_Y (T_CSTYLE_Y)
#define T_CCOLOR_W (117) // 117 lines this up perfectly with the default colors group box
#define T_CCOLOR_R_W (T_CCOLOR_W-P_4-P_4)
#define T_CCOLOR_COLOR_X (T_CCOLOR_X+P_4)
#define T_CCOLOR_TEXT_X (T_CCOLOR_COLOR_X+COLOR_SIZE+P_1)
#define T_CCOLOR_EDIT_X (T_CCOLOR_TEXT_X+T_CCOLOR_TEXT_W)
#define T_CCOLOR_TEXT_W (T_COLORS_TEXT_W)
#define T_CCOLORS_EDIT_W (T_COLORS_EDIT_W)
#define T_CCOLORS_EDIT_H (T_COLORS_EDIT_H)
#define T_CCOLOR_R_Y (T_CSTYLE_R_3_Y)
#define T_CCOLOR_G_Y (T_CCOLOR_R_Y+T_CCOLORS_EDIT_H+P_0)
#define T_CCOLOR_B_Y (T_CCOLOR_G_Y+T_CCOLORS_EDIT_H+P_0)
#define T_CCOLOR_H T_CSTYLE_H

// terminal scrolling group box
#define T_SCROLL_X T_CSTYLE_X
#define T_SCROLL_Y (T_CCOLOR_Y+T_CCOLOR_H+P_1)
#define T_SCROLL_W 100
#define T_SCROLL_H 40

#define UPDOWN_STYLES (UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ARROWKEYS | UDS_NOTHOUSANDS | UDS_ALIGNRIGHT)
DID_TERMINAL DIALOG  0, 0, 245, 226
CAPTION " Terminal "
STYLE WS_VISIBLE | WS_CAPTION | WS_CHILD | DS_MODALFRAME
FONT 8,"MS Shell Dlg"
BEGIN

    // GROUPBOX text, id, x, y, width, height [, style [, extended-style]]
    // CONTROL text, id, class, style, x, y, width, height [, extended-style]

    GROUPBOX        "Terminal Colors", -1, T_COLORS_X, T_COLORS_Y, T_COLORS_W, T_COLORS_H, WS_GROUP

    AUTOCHECKBOX    "Use Separate Foreground", IDD_USE_TERMINAL_FG, T_COLORS_X+P_1, T_COLORS_CHECK_Y, T_COLORS_FG_W, 10

    CONTROL         "", IDD_TERMINAL_FGCOLOR, "SimpleColor", WS_BORDER | WS_CHILD | WS_GROUP ,
                    T_COLORS_X+P_2, T_COLORS_RED_Y, COLOR_SIZE, COLOR_SIZE

    LTEXT           "Red:", -1, T_COLORS_FG_TEXT_X, T_COLORS_RED_Y, T_COLORS_TEXT_W, 9
    EDITTEXT        IDD_TERMINAL_FG_RED, T_COLORS_FG_INPUT_X, T_COLORS_RED_Y, T_COLORS_EDIT_W, T_COLORS_EDIT_H, WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL
    CONTROL         "", IDD_TERMINAL_FG_REDSCROLL, UPDOWN_CLASS,
                    UPDOWN_STYLES, 0, 0, 0, 0

    LTEXT           "Green:", -1, T_COLORS_FG_TEXT_X, T_COLORS_GREEN_Y, T_COLORS_TEXT_W, 9
    EDITTEXT        IDD_TERMINAL_FG_GREEN, T_COLORS_FG_INPUT_X, T_COLORS_GREEN_Y, T_COLORS_EDIT_W, T_COLORS_EDIT_H, WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL
    CONTROL         "", IDD_TERMINAL_FG_GREENSCROLL, UPDOWN_CLASS,
                    UPDOWN_STYLES, 0, 0, 0, 0

    LTEXT           "Blue:", -1, T_COLORS_FG_TEXT_X, T_COLORS_BLUE_Y, T_COLORS_TEXT_W, 9
    EDITTEXT        IDD_TERMINAL_FG_BLUE, T_COLORS_FG_INPUT_X, T_COLORS_BLUE_Y, T_COLORS_EDIT_W, T_COLORS_EDIT_H, WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL
    CONTROL         "", IDD_TERMINAL_FG_BLUESCROLL, UPDOWN_CLASS,
                    UPDOWN_STYLES, 0, 0, 0, 0

    AUTOCHECKBOX    "Use Separate Background", IDD_USE_TERMINAL_BG, T_COLORS_BG_X, T_COLORS_CHECK_Y, T_COLORS_FG_W, 10

    CONTROL         "", IDD_TERMINAL_BGCOLOR, "SimpleColor", WS_BORDER | WS_CHILD | WS_GROUP ,
                    T_COLORS_BG_X, T_COLORS_RED_Y, COLOR_SIZE, COLOR_SIZE

    LTEXT           "Red:", -1, T_COLORS_BG_TEXT_X, T_COLORS_RED_Y, T_COLORS_TEXT_W, 9
    EDITTEXT        IDD_TERMINAL_BG_RED, T_COLORS_BG_INPUT_X, T_COLORS_RED_Y, T_COLORS_EDIT_W, T_COLORS_EDIT_H, WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL
    CONTROL         "", IDD_TERMINAL_BG_REDSCROLL, UPDOWN_CLASS,
                    UPDOWN_STYLES, 0, 0, 0, 0

    LTEXT           "Green:", -1, T_COLORS_BG_TEXT_X, T_COLORS_GREEN_Y, T_COLORS_TEXT_W, 9
    EDITTEXT        IDD_TERMINAL_BG_GREEN, T_COLORS_BG_INPUT_X, T_COLORS_GREEN_Y, T_COLORS_EDIT_W, T_COLORS_EDIT_H, WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL
    CONTROL         "", IDD_TERMINAL_BG_GREENSCROLL, UPDOWN_CLASS,
                    UPDOWN_STYLES, 0, 0, 0, 0

    LTEXT           "Blue:", -1, T_COLORS_BG_TEXT_X, T_COLORS_BLUE_Y, T_COLORS_TEXT_W, 9
    EDITTEXT        IDD_TERMINAL_BG_BLUE, T_COLORS_BG_INPUT_X, T_COLORS_BLUE_Y, T_COLORS_EDIT_W, T_COLORS_EDIT_H, WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL
    CONTROL         "", IDD_TERMINAL_BG_BLUESCROLL, UPDOWN_CLASS,
                    UPDOWN_STYLES, 0, 0, 0, 0


    GROUPBOX        "Cursor Shape", -1, T_CSTYLE_X, T_CSTYLE_Y, T_CSTYLE_W, T_CSTYLE_H
    AUTORADIOBUTTON "Use Legacy Style", IDD_TERMINAL_LEGACY_CURSOR, T_CSTYLE_X+P_1, T_CSTYLE_R_1_Y, T_CSTYLE_R_W, T_CSTYLE_R_H, WS_TABSTOP|WS_GROUP
    AUTORADIOBUTTON "Underscore", IDD_TERMINAL_UNDERSCORE, T_CSTYLE_X+P_1, T_CSTYLE_R_2_Y, T_CSTYLE_R_W, T_CSTYLE_R_H,
    AUTORADIOBUTTON "Vertical Bar", IDD_TERMINAL_VERTBAR,  T_CSTYLE_X+P_1, T_CSTYLE_R_3_Y, T_CSTYLE_R_W, T_CSTYLE_R_H,
    AUTORADIOBUTTON "Empty Box", IDD_TERMINAL_EMPTYBOX,    T_CSTYLE_X+P_1, T_CSTYLE_R_4_Y, T_CSTYLE_R_W, T_CSTYLE_R_H,
    AUTORADIOBUTTON "Solid Box", IDD_TERMINAL_SOLIDBOX,    T_CSTYLE_X+P_1, T_CSTYLE_R_5_Y, T_CSTYLE_R_W, T_CSTYLE_R_H,


    GROUPBOX        "Cursor Colors", -1, T_CCOLOR_X, T_CCOLOR_Y, T_CCOLOR_W, T_CCOLOR_H, WS_GROUP

    AUTORADIOBUTTON "Inverse Color", IDD_TERMINAL_INVERSE_CURSOR, T_CCOLOR_X+P_1, T_CSTYLE_R_1_Y, T_CCOLOR_R_W, T_CSTYLE_R_H, WS_TABSTOP|WS_GROUP

    AUTORADIOBUTTON "Use Color", IDD_TERMINAL_CURSOR_USECOLOR, T_CCOLOR_X+P_1, T_CSTYLE_R_2_Y, T_CCOLOR_R_W, T_CSTYLE_R_H,

    CONTROL         "", IDD_TERMINAL_CURSOR_COLOR, "SimpleColor", WS_BORDER | WS_CHILD | WS_GROUP,
                    T_CCOLOR_X+P_2, T_CSTYLE_R_3_Y, COLOR_SIZE, COLOR_SIZE

    LTEXT           "Red:", -1, T_CCOLOR_TEXT_X, T_CCOLOR_R_Y, T_COLORS_TEXT_W, 9
    EDITTEXT        IDD_TERMINAL_CURSOR_RED, T_CCOLOR_EDIT_X, T_CCOLOR_R_Y, T_CCOLORS_EDIT_W, T_CCOLORS_EDIT_H, WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL
    CONTROL         "", IDD_TERMINAL_CURSOR_REDSCROLL, UPDOWN_CLASS, UPDOWN_STYLES, 0, 0, 0, 0

    LTEXT           "Green:", -1, T_CCOLOR_TEXT_X, T_CCOLOR_G_Y, T_COLORS_TEXT_W, 9
    EDITTEXT        IDD_TERMINAL_CURSOR_GREEN, T_CCOLOR_EDIT_X, T_CCOLOR_G_Y, T_CCOLORS_EDIT_W, T_CCOLORS_EDIT_H, WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL
    CONTROL         "", IDD_TERMINAL_CURSOR_GREENSCROLL, UPDOWN_CLASS, UPDOWN_STYLES, 0, 0, 0, 0

    LTEXT           "Blue:", -1, T_CCOLOR_TEXT_X, T_CCOLOR_B_Y, T_COLORS_TEXT_W, 9
    EDITTEXT        IDD_TERMINAL_CURSOR_BLUE, T_CCOLOR_EDIT_X, T_CCOLOR_B_Y, T_CCOLORS_EDIT_W, T_CCOLORS_EDIT_H, WS_TABSTOP | WS_GROUP | ES_AUTOHSCROLL
    CONTROL         "", IDD_TERMINAL_CURSOR_BLUESCROLL, UPDOWN_CLASS, UPDOWN_STYLES, 0, 0, 0, 0


    GROUPBOX        "Terminal Scrolling", -1, T_SCROLL_X, T_SCROLL_Y, T_SCROLL_W, T_SCROLL_H
    AUTOCHECKBOX    "Disable Scroll-Forward", IDD_DISABLE_SCROLLFORWARD, T_SCROLL_X+P_1, T_SCROLL_Y+P_2, T_SCROLL_W-P_4-P_4, 10

    CONTROL         "Find out more about <A HREF=""https://go.microsoft.com/fwlink/?linkid=2028595"">experimental terminal settings</A>",
                    IDD_HELP_TERMINAL, "SysLink", WS_TABSTOP, 10, 225, 200, 10
END


//
//  Strings
//

STRINGTABLE PRELOAD
BEGIN
    IDS_NAME,                    "Console"
    IDS_INFO,                    "Configures console properties."
    IDS_TITLE,                   "Console Windows"
    IDS_RASTERFONT,              "Raster Fonts"
    IDS_FONTSIZE,                "Point size should be between %d and %d"
    IDS_SELECTEDFONT,            "Selected Font"
    IDS_LINKERRCAP,              "Error Updating Shortcut"
    IDS_LINKERROR,               "Unable to modify the shortcut:\n%s.\nCheck to make sure it has not been deleted or renamed."
    IDS_TOOLTIP_LINE_SELECTION,  "Instead of being rectangular, text selection wraps lines."
    IDS_TOOLTIP_FILTER_ON_PASTE, "When pasting, remove tabs and convert smart quotes to regular quotes."
    IDS_TOOLTIP_LINE_WRAP,       "When resizing the window, wrap text to fit width."
    IDS_TOOLTIP_CTRL_KEYS,       "Allow new Ctrl-key shortcuts (may interfere with some applications)."
    IDS_TOOLTIP_EDIT_KEYS,       "Enable enhanced keyboard editing on command line."
    IDS_TOOLTIP_OPACITY,         "Adjust the transparency of the console window."
    IDS_TOOLTIP_INTERCEPT_COPY_PASTE,   "Use Ctrl+Shift+C/V as copy/paste shortcuts, regardless of input mode"
END


//
// Version resource information
//

#define VER_FILETYPE                VFT_DLL
#define VER_FILESUBTYPE             VFT2_UNKNOWN
#define VER_FILEDESCRIPTION_STR     "Control Panel Console Applet"
#define VER_INTERNALNAME_STR        "Console\0"
#define VER_ORIGINALFILENAME_STR    "CONSOLE.DLL"

#ifndef EXTERNAL_BUILD
#include "common.ver"
#endif

//
// Bitmaps
//
BM_TRUETYPE_ICON    BITMAP           "..\\..\\res\\truetype.bmp"

