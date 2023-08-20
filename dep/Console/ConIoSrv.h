/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.
--*/

#pragma once

#include <ntlpcapi.h>

#define CIS_ALPC_PORT_NAME           L""

#define CIS_EVENT_TYPE_INPUT         (0)
#define CIS_EVENT_TYPE_FOCUS         (1)
#define CIS_EVENT_TYPE_FOCUS_ACK     (2)

#define CIS_MSG_TYPE_MAPVIRTUALKEY   (0)
#define CIS_MSG_TYPE_VKKEYSCAN       (1)
#define CIS_MSG_TYPE_GETKEYSTATE     (2)

#define CIS_MSG_TYPE_GETDISPLAYSIZE  (3)
#define CIS_MSG_TYPE_GETFONTSIZE     (4)
#define CIS_MSG_TYPE_SETCURSOR       (5)
#define CIS_MSG_TYPE_UPDATEDISPLAY   (6)

#define CIS_MSG_ATTR_FLAGS           (0)

#define CIS_MSG_ATTR_BUFFER_SIZE     (1024)

#define CIS_DISPLAY_MODE_NONE        (0)
#define CIS_DISPLAY_MODE_BGFX        (1)
#define CIS_DISPLAY_MODE_DIRECTX     (2)

typedef struct {
    PORT_MESSAGE AlpcHeader;
    UCHAR Type;

    union {
        struct {
            UINT Code;
            UINT MapType;
            UINT ReturnValue;
        } MapVirtualKeyParams;

        struct {
            WCHAR Character;
            SHORT ReturnValue;
        } VkKeyScanParams;

        struct {
            int VirtualKey;
            SHORT ReturnValue;
        } GetKeyStateParams;

        struct {
            CD_IO_DISPLAY_SIZE DisplaySize;

            NTSTATUS ReturnValue;
        } GetDisplaySizeParams;

        struct {
            CD_IO_FONT_SIZE FontSize;

            NTSTATUS ReturnValue;
        } GetFontSizeParams;

        struct {
            CD_IO_CURSOR_INFORMATION CursorInformation;

            NTSTATUS ReturnValue;
        } SetCursorParams;

        struct {
            SHORT RowIndex;

            NTSTATUS ReturnValue;
        } UpdateDisplayParams;

        struct {
            USHORT DisplayMode;
        } GetDisplayModeParams;
    };
} CIS_MSG, *PCIS_MSG;

typedef struct {
    UCHAR Type;

    union {
        struct {
            INPUT_RECORD Record;
        } InputEvent;

        struct {
            BOOLEAN IsActive;
        } FocusEvent;
    };
} CIS_EVENT, *PCIS_EVENT;
