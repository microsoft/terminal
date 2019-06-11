// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
*  General rules for being installed in the Control Panel:
*
*      1) The CPL/DLL must export a function named CPlApplet which will handle
*         the messages discussed below.
*      2) If the applet needs to save information in CONTROL.INI minimize
*         clutter by using the application name [MMCPL.appletname].
*      3) If the applet is refrenced in CONTROL.INI under [MMCPL] use
*         the following form:
*              ...
*              [MMCPL]
*              uniqueName=c:\mydir\myapplet.dll
*              ...
*
*  The order applet CPLs/DLLs are loaded by Control Panel is not guaranteed.
*  They may be sorted for display, categorization, etc.
*
*/
#ifndef _INC_CPL
#define _INC_CPL

// clang-format off

#include <pshpack1.h>   /* Assume byte packing throughout */

#ifdef __cplusplus
extern "C" {            /* Assume C declarations for C++ */
#endif /* __cplusplus */

/* Deprecated; control.exe no longer uses these messages */
#define WM_CPL_LAUNCH   (WM_USER+1000)
#define WM_CPL_LAUNCHED (WM_USER+1001)

/* A function prototype for CPlApplet() */

typedef LONG (APIENTRY *APPLET_PROC)(HWND hwndCpl, UINT msg, LPARAM lParam1, LPARAM lParam2);

/* The data structure CPlApplet() must fill in. */

typedef struct tagCPLINFO
{
    int         idIcon;     /* icon resource id, provided by CPlApplet() */
    int         idName;     /* display name string resource id, provided by CPlApplet() */
    int         idInfo;     /* description/tooltip/status bar string resource id, provided by CPlApplet() */
    LONG_PTR    lData;      /* user defined data */
} CPLINFO, *LPCPLINFO;

typedef struct tagNEWCPLINFOA
{
    DWORD   dwSize;         /* size, in bytes, of the structure */
    DWORD   dwFlags;
    DWORD   dwHelpContext;  /* help context to use */
    LONG_PTR lData;         /* user defined data */
    HICON   hIcon;          /* icon to use, this is owned by the Control Panel window (may be deleted) */
    CHAR    szName[32];     /* display name */
    CHAR    szInfo[64];     /* description/tooltip/status bar string */
    CHAR    szHelpFile[128];/* path to help file to use */
} NEWCPLINFOA, *LPNEWCPLINFOA;
typedef struct tagNEWCPLINFOW
{
    DWORD   dwSize;         /* size, in bytes, of the structure */
    DWORD   dwFlags;
    DWORD   dwHelpContext;  /* help context to use */
    LONG_PTR lData;         /* user defined data */
    HICON   hIcon;          /* icon to use, this is owned by the Control Panel window (may be deleted) */
    WCHAR   szName[32];     /* display name */
    WCHAR   szInfo[64];     /* description/tooltip/status bar string */
    WCHAR   szHelpFile[128];/* path to help file to use */
} NEWCPLINFOW, *LPNEWCPLINFOW;
#ifdef UNICODE
typedef NEWCPLINFOW NEWCPLINFO;
typedef LPNEWCPLINFOW LPNEWCPLINFO;
#else
typedef NEWCPLINFOA NEWCPLINFO;
typedef LPNEWCPLINFOA LPNEWCPLINFO;
#endif // UNICODE

#if(WINVER >= 0x0400)
#define CPL_DYNAMIC_RES 0
/* This constant may be used in place of real resource IDs for the idIcon,
*  idName or idInfo members of the CPLINFO structure.  Normally, the system
*  uses these values to extract copies of the resources and store them in a
*  cache.  Once the resource information is in the cache, the system does not
*  need to load a CPL unless the user actually tries to use it.
*  CPL_DYNAMIC_RES tells the system not to cache the resource, but instead to
*  load the CPL every time it needs to display information about an item.  This
*  allows a CPL to dynamically decide what information will be displayed, but
*  is SIGNIFICANTLY SLOWER than displaying information from a cache.
*  Typically, CPL_DYNAMIC_RES is used when a control panel must inspect the
*  runtime status of some device in order to provide text or icons to display.
*  It should be avoided if possible because of the performance hit to Control Panel.
*/

#endif /* WINVER >= 0x0400 */

/* The messages CPlApplet() must handle: */

#define CPL_INIT        1
/*  This message is sent to indicate CPlApplet() was found. */
/*  lParam1 and lParam2 are not defined. */
/*  Return TRUE or FALSE indicating whether the control panel should proceed. */


#define CPL_GETCOUNT    2
/*  This message is sent to determine the number of applets to be displayed. */
/*  lParam1 and lParam2 are not defined. */
/*  Return the number of applets you wish to display in the control */
/*  panel window. */


#define CPL_INQUIRE     3
/*  This message is sent for information about each applet. */
/*  The return value is ignored. */
/*  lParam1 is the applet number to register, a value from 0 to */
/*  (CPL_GETCOUNT - 1).  lParam2 is a pointer to a CPLINFO structure. */
/*  Fill in CPLINFO's idIcon, idName, idInfo and lData fields with */
/*  the resource id for an icon to display, name and description string ids, */
/*  and a long data item associated with applet #lParam1.  This information */
/*  may be cached by the caller at runtime and/or across sessions. */
/*  To prevent caching, see CPL_DYNAMIC_RES, above.  If the icon, name, and description */
/*  are not dynamic then CPL_DYNAMIC_RES should not be used and the CPL_NEWINQURE message */
/*  should be ignored */


#define CPL_SELECT      4
/*  The CPL_SELECT message is not used. */


#define CPL_DBLCLK      5
/*  This message is sent when the applet's icon has been double-clicked. */
/*  lParam1 is the applet number which was selected. */
/*  lParam2 is the applet's lData value. */
/*  This message should initiate the applet's dialog box. */


#define CPL_STOP        6
/*  This message is sent for each applet when the control panel is exiting. */
/*  lParam1 is the applet number.  lParam2 is the applet's lData value. */
/*  Do applet specific cleaning up here. */


#define CPL_EXIT        7
/*  This message is sent just before the control panel calls FreeLibrary. */
/*  lParam1 and lParam2 are not defined. */
/*  Do non-applet specific cleaning up here. */


#define CPL_NEWINQUIRE    8
/*  Same as CPL_INQUIRE execpt lParam2 is a pointer to a NEWCPLINFO struct. */
/*  The return value is ignored. */
/*  A CPL should NOT respond to the CPL_NEWINQURE message unless CPL_DYNAMIC_RES */
/*  is used in CPL_INQUIRE.  CPLs which respond to CPL_NEWINQUIRE cannot be cached */
/*  and slow the loading of the Control Panel window. */

#if(WINVER >= 0x0400)
#define CPL_STARTWPARMSA 9
#define CPL_STARTWPARMSW 10
#ifdef UNICODE
#define CPL_STARTWPARMS CPL_STARTWPARMSW
#else
#define CPL_STARTWPARMS CPL_STARTWPARMSA
#endif
/* This message parallels CPL_DBLCLK in that the applet should initiate
*  its dialog box.  Where it differs is that this invocation is coming
*  out of RUNDLL, and there may be some extra directions for execution.
*  lParam1: the applet number.
*  lParam2: an LPSTR to any extra directions that might exist.
*  returns: TRUE if the message was handled; FALSE if not.
*/
#endif /* WINVER >= 0x0400 */

/* This message is internal to the Control Panel and MAIN applets.  */
/* It is only sent when an applet is invoked from the command line  */
/* during system installation.                                      */
#define CPL_SETUP               200

#ifdef __cplusplus
}
#endif    /* __cplusplus */

#include <poppack.h>

// clang-format on

#endif /* _INC_CPL */
