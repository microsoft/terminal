/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    condrv.h

Abstract:

    This module contains the declarations shared by the console driver and the
    user-mode components that use it.

Author:

    Wedson Almeida Filho (wedsonaf) 24-Sep-2009

Environment:

    Kernel and user modes.

--*/

#pragma once

#include "..\NT\ntioapi_x.h"

//
// Messages that can be received by servers, used in CD_IO_DESCRIPTOR::Function.
//

#define CONSOLE_IO_CONNECT        0x01
#define CONSOLE_IO_DISCONNECT     0x02
#define CONSOLE_IO_CREATE_OBJECT  0x03
#define CONSOLE_IO_CLOSE_OBJECT   0x04
#define CONSOLE_IO_RAW_WRITE      0x05
#define CONSOLE_IO_RAW_READ       0x06
#define CONSOLE_IO_USER_DEFINED   0x07
#define CONSOLE_IO_RAW_FLUSH      0x08

//
// Header of all IOs submitted to a server.
//

typedef struct _CD_IO_DESCRIPTOR {
    LUID Identifier;
    ULONG_PTR Process;
    ULONG_PTR Object;
    ULONG Function;
    ULONG InputSize;
    ULONG OutputSize;
    ULONG Reserved;
} CD_IO_DESCRIPTOR, *PCD_IO_DESCRIPTOR;

//
// Types of objects, used in CREATE_OBJECT_INFORMATION::ObjectType.
//

#define CD_IO_OBJECT_TYPE_CURRENT_INPUT   0x01
#define CD_IO_OBJECT_TYPE_CURRENT_OUTPUT  0x02
#define CD_IO_OBJECT_TYPE_NEW_OUTPUT      0x03
#define CD_IO_OBJECT_TYPE_GENERIC         0x04

//
// Payload of the CONSOLE_IO_CREATE_OBJECT io.
//

typedef struct _CD_CREATE_OBJECT_INFORMATION {
    ULONG ObjectType;
    ULONG ShareMode;
    ACCESS_MASK DesiredAccess;
} CD_CREATE_OBJECT_INFORMATION, *PCD_CREATE_OBJECT_INFORMATION;

//
// Create EA buffers.
//

#define CD_BROKER_EA_NAME "broker"
#define CD_SERVER_EA_NAME "server"
#define CD_ATTACH_EA_NAME "attach"

typedef struct _CD_CREATE_SERVER {
    HANDLE BrokerHandle;
    LUID BrokerRequest;
} CD_CREATE_SERVER, *PCD_CREATE_SERVER;

typedef struct _CD_ATTACH_INFORMATION {
    HANDLE ProcessId;
} CD_ATTACH_INFORMATION, *PCD_ATTACH_INFORMATION;

typedef struct _CD_ATTACH_INFORMATION64 {
    PVOID64 ProcessId;
} CD_ATTACH_INFORMATION64, *PCD_ATTACH_INFORMATION64;

//
// Information passed to the driver by a server when a connection is accepted.
//

typedef struct _CD_CONNECTION_INFORMATION {
    ULONG_PTR Process;
    ULONG_PTR Input;
    ULONG_PTR Output;
} CD_CONNECTION_INFORMATION, *PCD_CONNECTION_INFORMATION;

//
// Ioctls.
//

typedef struct _CD_IO_BUFFER {
    ULONG Size;
    PVOID Buffer;
} CD_IO_BUFFER, *PCD_IO_BUFFER;

typedef struct _CD_IO_BUFFER64 {
    ULONG Size;
    PVOID64 Buffer;
} CD_IO_BUFFER64, *PCD_IO_BUFFER64;

typedef struct _CD_USER_DEFINED_IO {
    HANDLE Client;
    ULONG InputCount;
    ULONG OutputCount;
    CD_IO_BUFFER Buffers[ANYSIZE_ARRAY];
} CD_USER_DEFINED_IO, *PCD_USER_DEFINED_IO;

typedef struct _CD_USER_DEFINED_IO64 {
    PVOID64 Client;
    ULONG InputCount;
    ULONG OutputCount;
    CD_IO_BUFFER64 Buffers[ANYSIZE_ARRAY];
} CD_USER_DEFINED_IO64, *PCD_USER_DEFINED_IO64;

typedef struct _CD_IO_BUFFER_DESCRIPTOR {
    PVOID Data;
    ULONG Size;
    ULONG Offset;
} CD_IO_BUFFER_DESCRIPTOR, *PCD_IO_BUFFER_DESCRIPTOR;

typedef struct _CD_IO_COMPLETE {
    LUID Identifier;
    IO_STATUS_BLOCK IoStatus;
    CD_IO_BUFFER_DESCRIPTOR Write;
} CD_IO_COMPLETE, *PCD_IO_COMPLETE;

typedef struct _CD_IO_OPERATION {
    LUID Identifier;
    CD_IO_BUFFER_DESCRIPTOR Buffer;
} CD_IO_OPERATION, *PCD_IO_OPERATION;

typedef struct _CD_IO_SERVER_INFORMATION {
    HANDLE InputAvailableEvent;
} CD_IO_SERVER_INFORMATION, *PCD_IO_SERVER_INFORMATION;

typedef struct _CD_IO_DISPLAY_SIZE {
    ULONG Width;
    ULONG Height;
} CD_IO_DISPLAY_SIZE, *PCD_IO_DISPLAY_SIZE;

typedef struct _CD_IO_CHARACTER {
    WCHAR Character; 
    USHORT Atribute;
} CD_IO_CHARACTER, *PCD_IO_CHARACTER;

typedef struct _CD_IO_ROW_INFORMATION {
    SHORT Index;
    PCD_IO_CHARACTER Old;
    PCD_IO_CHARACTER New;
} CD_IO_ROW_INFORMATION, *PCD_IO_ROW_INFORMATION;

typedef struct _CD_IO_CURSOR_INFORMATION {
    USHORT Column;
    USHORT Row;
    ULONG Height;
    BOOLEAN IsVisible;
} CD_IO_CURSOR_INFORMATION, *PCD_IO_CURSOR_INFORMATION;

#define IOCTL_CONDRV_READ_IO \
    CTL_CODE(FILE_DEVICE_CONSOLE, 1, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_COMPLETE_IO \
    CTL_CODE(FILE_DEVICE_CONSOLE, 2, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_READ_INPUT \
    CTL_CODE(FILE_DEVICE_CONSOLE, 3, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_WRITE_OUTPUT \
    CTL_CODE(FILE_DEVICE_CONSOLE, 4, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_ISSUE_USER_IO \
    CTL_CODE(FILE_DEVICE_CONSOLE, 5, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_DISCONNECT_PIPE \
    CTL_CODE(FILE_DEVICE_CONSOLE, 6, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_SET_SERVER_INFORMATION \
    CTL_CODE(FILE_DEVICE_CONSOLE, 7, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_GET_SERVER_PID \
    CTL_CODE(FILE_DEVICE_CONSOLE, 8, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_GET_DISPLAY_SIZE \
    CTL_CODE(FILE_DEVICE_CONSOLE, 9, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_UPDATE_DISPLAY \
    CTL_CODE(FILE_DEVICE_CONSOLE, 10, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_SET_CURSOR \
    CTL_CODE(FILE_DEVICE_CONSOLE, 11, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_ALLOW_VIA_UIACCESS \
    CTL_CODE(FILE_DEVICE_CONSOLE, 12, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_CONDRV_LAUNCH_SERVER \
    CTL_CODE(FILE_DEVICE_CONSOLE, 13, METHOD_NEITHER, FILE_ANY_ACCESS)
