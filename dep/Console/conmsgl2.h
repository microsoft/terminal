/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    conmsgl2.h

Abstract:

    This include file defines the layer 2 message formats used to communicate
    between the client and server portions of the CONSOLE portion of the
    Windows subsystem.

Author:

    Therese Stowell (thereses) 10-Nov-1990

Revision History:

    Wedson Almeida Filho (wedsonaf) 23-May-2010
        Modified the messages for use with the console driver.

--*/

#pragma once

typedef struct _CONSOLE_CREATESCREENBUFFER_MSG {
    IN ULONG Flags;
    IN ULONG BitmapInfoLength;
    IN ULONG Usage;
} CONSOLE_CREATESCREENBUFFER_MSG, *PCONSOLE_CREATESCREENBUFFER_MSG;

#define CONSOLE_ASCII             0x1
#define CONSOLE_REAL_UNICODE      0x2
#define CONSOLE_ATTRIBUTE         0x3
#define CONSOLE_FALSE_UNICODE     0x4

typedef struct _CONSOLE_FILLCONSOLEOUTPUT_MSG {
    IN COORD WriteCoord;
    IN ULONG ElementType;
    IN USHORT Element;
    IN OUT ULONG Length;
} CONSOLE_FILLCONSOLEOUTPUT_MSG, *PCONSOLE_FILLCONSOLEOUTPUT_MSG;

typedef struct _CONSOLE_CTRLEVENT_MSG {
    IN ULONG CtrlEvent;
    IN ULONG ProcessGroupId;
} CONSOLE_CTRLEVENT_MSG, *PCONSOLE_CTRLEVENT_MSG;

typedef struct _CONSOLE_SETCP_MSG {
    IN ULONG CodePage;
    IN BOOLEAN Output;
} CONSOLE_SETCP_MSG, *PCONSOLE_SETCP_MSG;

typedef struct _CONSOLE_GETCURSORINFO_MSG {
    OUT ULONG CursorSize;
    OUT BOOLEAN Visible;
} CONSOLE_GETCURSORINFO_MSG, *PCONSOLE_GETCURSORINFO_MSG;

typedef struct _CONSOLE_SETCURSORINFO_MSG {
    IN ULONG CursorSize;
    IN BOOLEAN Visible;
} CONSOLE_SETCURSORINFO_MSG, *PCONSOLE_SETCURSORINFO_MSG;

typedef struct _CONSOLE_SCREENBUFFERINFO_MSG {
    IN OUT COORD Size;
    IN OUT COORD CursorPosition;
    IN OUT COORD ScrollPosition;
    IN OUT USHORT Attributes;
    IN OUT COORD CurrentWindowSize;
    IN OUT COORD MaximumWindowSize;
    IN OUT USHORT PopupAttributes;
    IN OUT BOOLEAN FullscreenSupported;
    IN OUT COLORREF ColorTable[16];
} CONSOLE_SCREENBUFFERINFO_MSG, *PCONSOLE_SCREENBUFFERINFO_MSG;

typedef struct _CONSOLE_SETSCREENBUFFERSIZE_MSG {
    IN COORD Size;
} CONSOLE_SETSCREENBUFFERSIZE_MSG, *PCONSOLE_SETSCREENBUFFERSIZE_MSG;

typedef struct _CONSOLE_SETCURSORPOSITION_MSG {
    IN COORD CursorPosition;
} CONSOLE_SETCURSORPOSITION_MSG, *PCONSOLE_SETCURSORPOSITION_MSG;

typedef struct _CONSOLE_GETLARGESTWINDOWSIZE_MSG {
    OUT COORD Size;
} CONSOLE_GETLARGESTWINDOWSIZE_MSG, *PCONSOLE_GETLARGESTWINDOWSIZE_MSG;

typedef struct _CONSOLE_SCROLLSCREENBUFFER_MSG {
    IN SMALL_RECT ScrollRectangle;
    IN SMALL_RECT ClipRectangle;
    IN BOOLEAN Clip;
    IN BOOLEAN Unicode;
    IN COORD DestinationOrigin;
    IN CHAR_INFO Fill;
} CONSOLE_SCROLLSCREENBUFFER_MSG, *PCONSOLE_SCROLLSCREENBUFFER_MSG;

typedef struct _CONSOLE_SETTEXTATTRIBUTE_MSG {
    IN USHORT Attributes;
} CONSOLE_SETTEXTATTRIBUTE_MSG, *PCONSOLE_SETTEXTATTRIBUTE_MSG;

typedef struct _CONSOLE_SETWINDOWINFO_MSG {
    IN BOOLEAN Absolute;
    IN SMALL_RECT Window;
} CONSOLE_SETWINDOWINFO_MSG, *PCONSOLE_SETWINDOWINFO_MSG;

typedef struct _CONSOLE_READCONSOLEOUTPUTSTRING_MSG {
    IN COORD ReadCoord;
    IN ULONG StringType;
    OUT ULONG NumRecords;
} CONSOLE_READCONSOLEOUTPUTSTRING_MSG, *PCONSOLE_READCONSOLEOUTPUTSTRING_MSG;

typedef struct _CONSOLE_WRITECONSOLEINPUT_MSG {
    OUT ULONG NumRecords;
    IN BOOLEAN Unicode;
    IN BOOLEAN Append;
} CONSOLE_WRITECONSOLEINPUT_MSG, *PCONSOLE_WRITECONSOLEINPUT_MSG;

typedef struct _CONSOLE_WRITECONSOLEOUTPUTSTRING_MSG {
    IN COORD WriteCoord;
    IN ULONG StringType;
    OUT ULONG NumRecords;
} CONSOLE_WRITECONSOLEOUTPUTSTRING_MSG, *PCONSOLE_WRITECONSOLEOUTPUTSTRING_MSG;

typedef struct _CONSOLE_WRITECONSOLEOUTPUT_MSG {
    IN OUT SMALL_RECT CharRegion;
    IN BOOLEAN Unicode;
} CONSOLE_WRITECONSOLEOUTPUT_MSG, *PCONSOLE_WRITECONSOLEOUTPUT_MSG;

typedef struct _CONSOLE_READCONSOLEOUTPUT_MSG {
    IN OUT SMALL_RECT CharRegion;
    IN BOOLEAN Unicode;
} CONSOLE_READCONSOLEOUTPUT_MSG, *PCONSOLE_READCONSOLEOUTPUT_MSG;

typedef struct _CONSOLE_GETTITLE_MSG {
    OUT ULONG TitleLength;
    IN BOOLEAN Unicode;
    IN BOOLEAN Original;
} CONSOLE_GETTITLE_MSG, *PCONSOLE_GETTITLE_MSG;

typedef struct _CONSOLE_SETTITLE_MSG {
    IN BOOLEAN Unicode;
} CONSOLE_SETTITLE_MSG, *PCONSOLE_SETTITLE_MSG;

typedef enum _CONSOLE_API_NUMBER_L2 {
    ConsolepFillConsoleOutput = CONSOLE_FIRST_API_NUMBER(2),
    ConsolepGenerateCtrlEvent,
    ConsolepSetActiveScreenBuffer,
    ConsolepFlushInputBuffer,
    ConsolepSetCP,
    ConsolepGetCursorInfo,
    ConsolepSetCursorInfo,
    ConsolepGetScreenBufferInfo,
    ConsolepSetScreenBufferInfo,
    ConsolepSetScreenBufferSize,
    ConsolepSetCursorPosition,
    ConsolepGetLargestWindowSize,
    ConsolepScrollScreenBuffer,
    ConsolepSetTextAttribute,
    ConsolepSetWindowInfo,
    ConsolepReadConsoleOutputString,
    ConsolepWriteConsoleInput,
    ConsolepWriteConsoleOutput,
    ConsolepWriteConsoleOutputString,
    ConsolepReadConsoleOutput,
    ConsolepGetTitle,
    ConsolepSetTitle,
} CONSOLE_API_NUMBER_L2, *PCONSOLE_API_NUMBER_L2;

typedef union _CONSOLE_MSG_BODY_L2 {
    CONSOLE_CTRLEVENT_MSG GenerateConsoleCtrlEvent;
    CONSOLE_FILLCONSOLEOUTPUT_MSG FillConsoleOutput;
    CONSOLE_SETCP_MSG SetConsoleCP;
    CONSOLE_GETCURSORINFO_MSG GetConsoleCursorInfo;
    CONSOLE_SETCURSORINFO_MSG SetConsoleCursorInfo;
    CONSOLE_SCREENBUFFERINFO_MSG GetConsoleScreenBufferInfo;
    CONSOLE_SCREENBUFFERINFO_MSG SetConsoleScreenBufferInfo;
    CONSOLE_SETSCREENBUFFERSIZE_MSG SetConsoleScreenBufferSize;
    CONSOLE_SETCURSORPOSITION_MSG SetConsoleCursorPosition;
    CONSOLE_GETLARGESTWINDOWSIZE_MSG GetLargestConsoleWindowSize;
    CONSOLE_SCROLLSCREENBUFFER_MSG ScrollConsoleScreenBuffer;
    CONSOLE_SETTEXTATTRIBUTE_MSG SetConsoleTextAttribute;
    CONSOLE_SETWINDOWINFO_MSG SetConsoleWindowInfo;
    CONSOLE_READCONSOLEOUTPUTSTRING_MSG ReadConsoleOutputString;
    CONSOLE_WRITECONSOLEINPUT_MSG WriteConsoleInput;
    CONSOLE_WRITECONSOLEOUTPUTSTRING_MSG WriteConsoleOutputString;
    CONSOLE_WRITECONSOLEOUTPUT_MSG WriteConsoleOutput;
    CONSOLE_READCONSOLEOUTPUT_MSG ReadConsoleOutput;
    CONSOLE_SETTITLE_MSG SetConsoleTitle;
    CONSOLE_GETTITLE_MSG GetConsoleTitle;
} CONSOLE_MSG_BODY_L2, *PCONSOLE_MSG_BODY_L2;

#ifndef __cplusplus
typedef struct _CONSOLE_MSG_L2 {
    CONSOLE_MSG_HEADER Header;
    union {
        CONSOLE_MSG_BODY_L2;
    } u;
} CONSOLE_MSG_L2, *PCONSOLE_MSG_L2;
#else
typedef struct _CONSOLE_MSG_L2 :
    public CONSOLE_MSG_HEADER
{
    CONSOLE_MSG_BODY_L2 u;
} CONSOLE_MSG_L2, *PCONSOLE_MSG_L2;
#endif // __cplusplus
