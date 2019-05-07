/****************************** Module Header ******************************\
* Module Name: res.rc
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Constants
*
* History:
* 08-21-91      Created.
\***************************************************************************/

#include <windows.h>
#include "resource.h"

#ifndef EXTERNAL_BUILD
#include "conhost.rcv"
#include <ntverp.h>
#include <common.ver>
#endif

IDI_APPICON             ICON    "..\\..\\..\\res\\console.ico"

#ifndef EXTERNAL_BUILD
// Fusion default manifest (needed since we are created w/ RtlCreateUserProcess)
IDR_SYSTEM_MANIFEST     RT_MANIFEST MOVEABLE    PURE    "SystemDefault.man"
#endif

//
//  Menus
//

ID_CONSOLE_SYSTEMMENU MENU PRELOAD
BEGIN
  MENUITEM    "Mar&k\tCtrl-M",        ID_CONSOLE_MARK
  MENUITEM    "Cop&y\tEnter", ID_CONSOLE_COPY
  MENUITEM    "&Paste\tCtrl-V",       ID_CONSOLE_PASTE
  MENUITEM    "&Select All\tCtrl-A",  ID_CONSOLE_SELECTALL
  MENUITEM    "Scro&ll",      ID_CONSOLE_SCROLL
  MENUITEM    "&Find...\tCtrl-F",     ID_CONSOLE_FIND
END

//
//  Strings
//

STRINGTABLE PRELOAD
BEGIN
/* errors */

    /*
     * Command line editing messages
     */

    ID_CONSOLE_MSGCMDLINEF2, "Enter char to copy up to: "
    ID_CONSOLE_MSGCMDLINEF4, "Enter char to delete up to: "
    ID_CONSOLE_MSGCMDLINEF9, "Enter command number: "

    ID_CONSOLE_MSGMARKMODE,     "Mark "
    ID_CONSOLE_MSGSELECTMODE,   "Select "
    ID_CONSOLE_MSGSCROLLMODE,   "Scroll "

    ID_CONSOLE_FMT_WINDOWTITLE, "%s%s"

/* WIP Audit destination name */
    ID_CONSOLE_WIP_DESTINATIONNAME, "console application"

/* Menu items that replace the standard ones. These don't have the accelerators */
    SC_CLOSE,       "&Close"

/* just menu items */
    ID_CONSOLE_CONTROL,  "&Properties"
    ID_CONSOLE_EDIT,     "&Edit"
    ID_CONSOLE_DEFAULTS, "&Defaults"
END

//
//  Dialogs
//

ID_CONSOLE_FINDDLG DIALOG LOADONCALL MOVEABLE DISCARDABLE 30, 73, 236, 62
STYLE WS_BORDER | WS_CAPTION | DS_MODALFRAME | WS_POPUP | DS_3DLOOK | WS_SYSMENU
CAPTION "Find"
FONT 8, "MS Shell Dlg"
BEGIN
    LTEXT           "Fi&nd what:", -1, 4, 8, 42, 8
    EDITTEXT        ID_CONSOLE_FINDSTR, 47, 7, 128, 12, WS_GROUP | WS_TABSTOP | ES_AUTOHSCROLL

    AUTOCHECKBOX    "Match &case", ID_CONSOLE_FINDCASE, 4, 42, 64, 12

    GROUPBOX        "Direction", -1, 107, 26, 68, 28, WS_GROUP
    AUTORADIOBUTTON "&Up", ID_CONSOLE_FINDUP, 111, 38, 25, 12, WS_GROUP
    AUTORADIOBUTTON "&Down", ID_CONSOLE_FINDDOWN, 138, 38, 35, 12

    DEFPUSHBUTTON   "&Find Next", IDOK, 182, 5, 50, 14, WS_GROUP
    PUSHBUTTON      "Cancel", IDCANCEL, 182, 23, 50, 14
END
