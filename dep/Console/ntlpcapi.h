/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.
--*/

#pragma once

#define OB_FILE_OBJECT_TYPE 1

typedef struct _PORT_MESSAGE {
    union {
        struct {
            SHORT DataLength;
            SHORT TotalLength;
        } s1;
    } u1;
    union {
        ULONG ZeroInit;
    } u2;
    union {
        CLIENT_ID ClientId;
    };
    ULONG MessageId;
} PORT_MESSAGE, *PPORT_MESSAGE;

#define ALPC_MSGFLG_SYNC_REQUEST            0
#define ALPC_PORFLG_ACCEPT_DUP_HANDLES      1
#define ALPC_PORFLG_ACCEPT_INDIRECT_HANDLES 2

typedef struct _ALPC_DATA_VIEW_ATTR {
    PVOID  ViewBase;
    SIZE_T ViewSize;
} ALPC_DATA_VIEW_ATTR, *PALPC_DATA_VIEW_ATTR;

typedef struct _ALPC_CONTEXT_ATTR {
} ALPC_CONTEXT_ATTR, *PALPC_CONTEXT_ATTR;

#define ALPC_INDIRECT_HANDLE_MAX 512

typedef struct _ALPC_HANDLE_ATTR {
    union {
        ULONG HandleCount;
    };
} ALPC_HANDLE_ATTR, *PALPC_HANDLE_ATTR;

#define ALPC_FLG_MSG_DATAVIEW_ATTR 1
#define ALPC_FLG_MSG_HANDLE_ATTR   2

typedef struct _ALPC_MESSAGE_ATTRIBUTES {
} ALPC_MESSAGE_ATTRIBUTES, *PALPC_MESSAGE_ATTRIBUTES;

typedef struct _ALPC_PORT_ATTRIBUTES {
    ULONG Flags;
    SECURITY_QUALITY_OF_SERVICE SecurityQos;
    SIZE_T MaxMessageLength;
    SIZE_T MemoryBandwidth;
    SIZE_T MaxPoolUsage;
    SIZE_T MaxSectionSize;
    SIZE_T MaxViewSize;
    SIZE_T MaxTotalSectionSize;
    ULONG DupObjectTypes;
#ifdef _WIN64
    ULONG Reserved;
#endif
} ALPC_PORT_ATTRIBUTES, *PALPC_PORT_ATTRIBUTES;

typedef enum _ALPC_MESSAGE_INFORMATION_CLASS {
    AlpcMessageHandleInformation
} ALPC_MESSAGE_INFORMATION_CLASS;

typedef struct _ALPC_MESSAGE_HANDLE_INFORMATION {
    ULONG Index;
    ULONG Handle;
} ALPC_MESSAGE_HANDLE_INFORMATION, *PALPC_MESSAGE_HANDLE_INFORMATION;

NTSTATUS AlpcInitializeMessageAttribute(
    ULONG AttributeFlags,
    PALPC_MESSAGE_ATTRIBUTES Buffer,
    SIZE_T BufferSize,
    PSIZE_T RequiredBufferSize
);

PVOID AlpcGetMessageAttribute(
    PALPC_MESSAGE_ATTRIBUTES Buffer,
    ULONG AttributeFlag
);

#define ALPC_GET_DATAVIEW_ATTRIBUTES(MsgAttr) \
    ((PALPC_DATA_VIEW_ATTR)AlpcGetMessageAttribute(MsgAttr, ALPC_FLG_MSG_DATAVIEW_ATTR))

#define ALPC_GET_HANDLE_ATTRIBUTES(MsgAttr) \
    ((PALPC_HANDLE_ATTR)AlpcGetMessageAttribute(MsgAttr, ALPC_FLG_MSG_HANDLE_ATTR))

NTSTATUS NtAlpcConnectPort(
    PHANDLE PortHandle,
    PUNICODE_STRING PortName,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PALPC_PORT_ATTRIBUTES PortAttributes,
    ULONG Flags,
    PSID RequiredServerSid,
    PPORT_MESSAGE ConnectionMessage,
    PSIZE_T BufferLength,
    PALPC_MESSAGE_ATTRIBUTES OutMessageAttributes,
    PALPC_MESSAGE_ATTRIBUTES InMessageAttributes,
    PLARGE_INTEGER Timeout
);

NTSTATUS NtAlpcSendWaitReceivePort(
    HANDLE PortHandle,
    ULONG Flags,
    PPORT_MESSAGE SendMessage,
    PALPC_MESSAGE_ATTRIBUTES SendMessageAttributes,
    PPORT_MESSAGE ReceiveMessage,
    PSIZE_T BufferLength,
    PALPC_MESSAGE_ATTRIBUTES ReceiveMessageAttributes,
    PLARGE_INTEGER Timeout
);

NTSTATUS NtAlpcQueryInformationMessage(
    HANDLE PortHandle,
    PPORT_MESSAGE PortMessage,
    ALPC_MESSAGE_INFORMATION_CLASS MessageInformationClass,
    PVOID MessageInformation,
    ULONG Length,
    PULONG ReturnLength
);
