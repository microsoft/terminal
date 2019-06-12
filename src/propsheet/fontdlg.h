/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    fontdlg.h

Abstract:

    This module contains the definitions for console font dialog

Author:

    Therese Stowell (thereses) Feb-3-1992 (swiped from Win3.1)

Revision History:

--*/

#pragma once

#ifndef FONTDLG_H
#define FONTDLG_H

/* ----- Literals ----- */

#define MAXDIMENSTRING 40 // max text in combo box
#define DX_TTBITMAP 20
#define DY_TTBITMAP 12
#define CCH_RASTERFONTS 24
#define CCH_SELECTEDFONT 30

/* ----- Macros ----- */
/*
 *  High-level macros
 *
 *  These macros handle the SendMessages that go tofrom list boxes
 *  and combo boxes.
 *
 *  The "xxx_lcb" prefix stands for leaves CritSect & "list or combo box".
 *
 *  Basically, we're providing mnemonic names for what would otherwise
 *  look like a whole slew of confusing SendMessage's.
 *
 */
#define lcbRESETCONTENT(hWnd, bLB) \
    SendMessage(hWnd, bLB ? LB_RESETCONTENT : CB_RESETCONTENT, 0, 0L)

#define lcbGETTEXT(hWnd, bLB, w) \
    SendMessage(hWnd, bLB ? LB_GETTEXT : CB_GETLBTEXT, w, 0L)

#define lcbFINDSTRINGEXACT(hWnd, bLB, pwsz) \
    (LONG) SendMessage(hWnd, bLB ? LB_FINDSTRINGEXACT : CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)pwsz)

#define lcbADDSTRING(hWnd, bLB, pwsz) \
    (LONG) SendMessage(hWnd, bLB ? LB_ADDSTRING : CB_ADDSTRING, 0, (LPARAM)pwsz)

#define lcbSETITEMDATA(hWnd, bLB, w, nFont) \
    SendMessage(hWnd, bLB ? LB_SETITEMDATA : CB_SETITEMDATA, w, nFont)

#define lcbGETITEMDATA(hWnd, bLB, w) \
    (LONG) SendMessage(hWnd, bLB ? LB_GETITEMDATA : CB_GETITEMDATA, w, 0L)

#define lcbGETCOUNT(hWnd, bLB) \
    (LONG) SendMessage(hWnd, bLB ? LB_GETCOUNT : CB_GETCOUNT, 0, 0L)

#define lcbGETCURSEL(hWnd, bLB) \
    (LONG) SendMessage(hWnd, bLB ? LB_GETCURSEL : CB_GETCURSEL, 0, 0L)

#define lcbSETCURSEL(hWnd, bLB, w) \
    SendMessage(hWnd, bLB ? LB_SETCURSEL : CB_SETCURSEL, w, 0L)

#endif /* #ifndef FONTDLG_H */
