/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    conmsgl1.h

Abstract:

    This include file defines the layer 1 message formats used to communicate
    between the client and server portions of the CONSOLE portion of the
    Windows subsystem.

Author:

    Therese Stowell (thereses) 10-Nov-1990

Revision History:

    Wedson Almeida Filho (wedsonaf) 23-May-2010
        Modified the messages for use with the console driver.

--*/

#pragma once

#define CONSOLE_FIRST_API_NUMBER(Layer) \
    (Layer << 24) \

typedef struct _CONSOLE_SERVER_MSG {
    ULONG IconId;
    ULONG HotKey;
    ULONG StartupFlags;
    USHORT FillAttribute;
    USHORT ShowWindow;
    COORD ScreenBufferSize;
    COORD WindowSize;
    COORD WindowOrigin;
    ULONG ProcessGroupId;
    BOOLEAN ConsoleApp;
    BOOLEAN WindowVisible;
    USHORT TitleLength;
    WCHAR Title[MAX_PATH + 1];
    USHORT ApplicationNameLength;
    WCHAR ApplicationName[128];
    USHORT CurrentDirectoryLength;
    WCHAR CurrentDirectory[MAX_PATH + 1];
} CONSOLE_SERVER_MSG, *PCONSOLE_SERVER_MSG;

typedef struct _CONSOLE_BROKER_DATA {
    WCHAR DesktopName[MAX_PATH];
} CONSOLE_BROKER_MSG, *PCONSOLE_BROKER_MSG;

typedef struct _CONSOLE_GETCP_MSG {
    OUT ULONG CodePage;
    IN BOOLEAN Output;
} CONSOLE_GETCP_MSG, *PCONSOLE_GETCP_MSG;

typedef struct _CONSOLE_MODE_MSG {
    IN OUT ULONG Mode;
} CONSOLE_MODE_MSG, *PCONSOLE_MODE_MSG;

typedef struct _CONSOLE_GETNUMBEROFINPUTEVENTS_MSG {
    OUT ULONG ReadyEvents;
} CONSOLE_GETNUMBEROFINPUTEVENTS_MSG, *PCONSOLE_GETNUMBEROFINPUTEVENTS_MSG;

typedef struct _CONSOLE_GETCONSOLEINPUT_MSG {
    OUT ULONG NumRecords;
    IN USHORT Flags;
    IN BOOLEAN Unicode;
} CONSOLE_GETCONSOLEINPUT_MSG, *PCONSOLE_GETCONSOLEINPUT_MSG;

typedef struct _CONSOLE_READCONSOLE_MSG {
    IN BOOLEAN Unicode;
    IN BOOLEAN ProcessControlZ;
    IN USHORT ExeNameLength;
    IN ULONG InitialNumBytes;
    IN ULONG CtrlWakeupMask;
    OUT ULONG ControlKeyState;
    OUT ULONG NumBytes;
} CONSOLE_READCONSOLE_MSG, *PCONSOLE_READCONSOLE_MSG;

typedef struct _CONSOLE_WRITECONSOLE_MSG {
    OUT ULONG NumBytes;
    IN BOOLEAN Unicode;
} CONSOLE_WRITECONSOLE_MSG, *PCONSOLE_WRITECONSOLE_MSG;

typedef struct _CONSOLE_LANGID_MSG {
    OUT LANGID LangId;
} CONSOLE_LANGID_MSG, *PCONSOLE_LANGID_MSG;

typedef struct _CONSOLE_MAPBITMAP_MSG {
    OUT HANDLE Mutex;
    OUT PVOID Bitmap;
} CONSOLE_MAPBITMAP_MSG, *PCONSOLE_MAPBITMAP_MSG;

typedef struct _CONSOLE_MAPBITMAP_MSG64 {
    OUT PVOID64 Mutex;
    OUT PVOID64 Bitmap;
} CONSOLE_MAPBITMAP_MSG64, *PCONSOLE_MAPBITMAP_MSG64;

typedef enum _CONSOLE_API_NUMBER_L1 {
    ConsolepGetCP = CONSOLE_FIRST_API_NUMBER(1),
    ConsolepGetMode,
    ConsolepSetMode,
    ConsolepGetNumberOfInputEvents,
    ConsolepGetConsoleInput,
    ConsolepReadConsole,
    ConsolepWriteConsole,
    ConsolepNotifyLastClose,
    ConsolepGetLangId,
    ConsolepMapBitmap,
} CONSOLE_API_NUMBER_L1, *PCONSOLE_API_NUMBER_L1;

typedef struct _CONSOLE_MSG_HEADER {
    ULONG ApiNumber;
    ULONG ApiDescriptorSize;
} CONSOLE_MSG_HEADER, *PCONSOLE_MSG_HEADER;

typedef union _CONSOLE_MSG_BODY_L1 {
    CONSOLE_GETCP_MSG GetConsoleCP;
    CONSOLE_MODE_MSG GetConsoleMode;
    CONSOLE_MODE_MSG SetConsoleMode;
    CONSOLE_GETNUMBEROFINPUTEVENTS_MSG GetNumberOfConsoleInputEvents;
    CONSOLE_GETCONSOLEINPUT_MSG GetConsoleInput;
    CONSOLE_READCONSOLE_MSG ReadConsole;
    CONSOLE_WRITECONSOLE_MSG WriteConsole;
    CONSOLE_LANGID_MSG GetConsoleLangId;

#if defined(BUILD_WOW6432) && !defined(BUILD_WOW3232)

    CONSOLE_MAPBITMAP_MSG64 MapBitmap;

#else 

    CONSOLE_MAPBITMAP_MSG MapBitmap;

#endif

} CONSOLE_MSG_BODY_L1, *PCONSOLE_MSG_BODY_L1;

#ifndef __cplusplus
typedef struct _CONSOLE_MSG_L1 {
    CONSOLE_MSG_HEADER Header;
    union {
        CONSOLE_MSG_BODY_L1;
    } u;
} CONSOLE_MSG_L1, *PCONSOLE_MSG_L1;
#else
typedef struct _CONSOLE_MSG_L1 :
    public CONSOLE_MSG_HEADER
{
    CONSOLE_MSG_BODY_L1 u;
} CONSOLE_MSG_L1, *PCONSOLE_MSG_L1;
#endif // __cplusplus
