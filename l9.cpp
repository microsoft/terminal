/*++
Copyright (c) Microsoft Corporation. All rights reserved.
Module Name:
    callserver.c
Abstract:
    This module contains the implementation of the routines used by console
    clients in all layers to call console servers.
Author:
    Wedson Almeida Filho (wedsonaf) 14-Jun-2010
Environment:
    User mode.
--*/

#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct/union

#include <windows.h>

/////
#define CONSOLE_FIRST_API_NUMBER(Layer) \
    (Layer << 24)

typedef struct _CD_IO_BUFFER
{
    ULONG Size;
    PVOID Buffer;
} CD_IO_BUFFER, *PCD_IO_BUFFER;

typedef struct _CD_IO_BUFFER64
{
    ULONG Size;
    PVOID64 Buffer;
} CD_IO_BUFFER64, *PCD_IO_BUFFER64;

typedef struct _CD_USER_DEFINED_IO
{
    HANDLE Client;
    ULONG InputCount;
    ULONG OutputCount;
    CD_IO_BUFFER Buffers[ANYSIZE_ARRAY];
} CD_USER_DEFINED_IO, *PCD_USER_DEFINED_IO;

typedef struct _CD_USER_DEFINED_IO64
{
    PVOID64 Client;
    ULONG InputCount;
    ULONG OutputCount;
    CD_IO_BUFFER64 Buffers[ANYSIZE_ARRAY];
} CD_USER_DEFINED_IO64, *PCD_USER_DEFINED_IO64;

typedef struct _CONSOLE_MSG_HEADER
{
    ULONG ApiNumber;
    ULONG ApiDescriptorSize;
} CONSOLE_MSG_HEADER, *PCONSOLE_MSG_HEADER;

typedef struct _CONSOLE_BUFFER
{
    ULONG Size;
    PVOID Buffer;
} CONSOLE_BUFFER, *PCONSOLE_BUFFER;

#define IOCTL_CONDRV_ISSUE_USER_IO \
    CTL_CODE(FILE_DEVICE_CONSOLE, 5, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#pragma warning(pop)


#pragma region L9

typedef enum _CONSOLE_API_NUMBER_L9 {
    ConsoleTestApi = CONSOLE_FIRST_API_NUMBER(9),
} CONSOLE_API_NUMBER_L9, *PCONSOLE_API_NUMBER_L9;

typedef struct _CONSOLE_L9_TEST_API {
    IN ULONG TestValue;
    OUT ULONG ReplyValue;
} CONSOLE_L9_TEST_API, *PCONSOLE_L9_TEST_API;

typedef union _CONSOLE_MSG_BODY_L9 {
    CONSOLE_L9_TEST_API TestApi;
} CONSOLE_MSG_BODY_L9, *PCONSOLE_MSG_BODY_L9;

#ifndef __cplusplus
typedef struct _CONSOLE_MSG_L9 {
    CONSOLE_MSG_HEADER Header;
    union {
        CONSOLE_MSG_BODY_L9;
    } u;
} CONSOLE_MSG_L9, *PCONSOLE_MSG_L9;
#else
typedef struct _CONSOLE_MSG_L9 :
    public CONSOLE_MSG_HEADER
{
    CONSOLE_MSG_BODY_L9 u;
} CONSOLE_MSG_L9, *PCONSOLE_MSG_L9;
#endif  // __cplusplus

#pragma endregion

HANDLE ConsoleGetHandle() noexcept
{
    return GetStdHandle(STD_OUTPUT_HANDLE);
}

HRESULT
ConsoleCallServerGeneric(
    __in HANDLE Handle,
    __in_opt HANDLE ClientHandle,
    __inout_bcount(sizeof(CONSOLE_MSG_HEADER) + ArgumentSize) PCONSOLE_MSG_HEADER Header,
    __in ULONG ApiNumber,
    __in ULONG ArgumentSize,
    __in_ecount_opt(InputCount) PCONSOLE_BUFFER InputBuffers,
    __in ULONG InputCount,
    __in_ecount_opt(OutputCount) PCONSOLE_BUFFER OutputBuffers,
    __in ULONG OutputCount)

/*++

Routine Description:

    This routine sends a request to the console server associated with the
    given console object.

Arguments:

    Handle - Supplies either a client handle (in which case ClientHandle
        should be NULL) or a connection handle (in which case ClientHandle
        may be non-NULL).

    ClientHandle - Supplies the console object whose server will receive the
        request, when Handle is a handle to a connection object.

    Header - Supplies the header of the contents of the request to be sent to
        the server. The actual payload must follow the header.

    ApiNumber - Supplies the number of request the be sent to the server.

    ArgumentSize - Supplies the size, in bytes, of the request payload that
        follows the header.

    InputBuffers - Optionally supplies an array of additional input buffers.

    InputCount - Supplies the number of elements in the input buffer array.

    OutputBuffers - Optionally supplies an array of additional output buffers.

    OutputCount - Supplies the number of elements in the output buffer array.

Return Value:

    HRESULT indicating the result of the call. The server determine the
    result.

--*/

{
    ULONG Count;
    struct
    {
#if defined(BUILD_WOW6432) && !defined(BUILD_WOW3232)

        CD_USER_DEFINED_IO64 Io;
        CD_IO_BUFFER64 Buffers[10];

#else

        CD_USER_DEFINED_IO Io;
        CD_IO_BUFFER Buffers[10];

#endif

    } Descriptors;
    ULONG Index;
    ULONG InSize;
    HRESULT Status;

    Count = InputCount + OutputCount + 2;
    if (Count > 10)
    {
        return E_INVALIDARG;
    }

    if ((LONG_PTR)Handle <= 0)
    {
        return E_INVALIDARG;
    }

    //
    // Initialize the input and ouput descriptors containing the message.
    //

    Header->ApiNumber = ApiNumber;
    Header->ApiDescriptorSize = ArgumentSize;

    Descriptors.Io.Buffers[0].Buffer = Header;
    Descriptors.Io.Buffers[0].Size = sizeof(CONSOLE_MSG_HEADER) + ArgumentSize;

    Descriptors.Io.Buffers[InputCount + 1].Buffer = Header + 1;
    Descriptors.Io.Buffers[InputCount + 1].Size = ArgumentSize;

    //
    // Initialize the potential additional descriptors and calculate the input
    // size.
    //

    Descriptors.Io.Client = ClientHandle;
    Descriptors.Io.InputCount = InputCount + 1;
    Descriptors.Io.OutputCount = OutputCount + 1;

    for (Index = 0; Index < InputCount; Index += 1)
    {
        Descriptors.Io.Buffers[Index + 1].Size = InputBuffers[Index].Size;
        Descriptors.Io.Buffers[Index + 1].Buffer = InputBuffers[Index].Buffer;
    }

    for (Index = 0; Index < OutputCount; Index += 1)
    {
        Descriptors.Io.Buffers[Index + InputCount + 2].Size =
            OutputBuffers[Index].Size;

        Descriptors.Io.Buffers[Index + InputCount + 2].Buffer =
            OutputBuffers[Index].Buffer;
    }

    InSize =

#if defined(BUILD_WOW6432) && !defined(BUILD_WOW3232)

        Count * sizeof(CD_IO_BUFFER64) + FIELD_OFFSET(CD_USER_DEFINED_IO64, Buffers);

#else

        Count * sizeof(CD_IO_BUFFER) + FIELD_OFFSET(CD_USER_DEFINED_IO, Buffers);

#endif

    //
    // Issue the io.
    //

    DWORD dwOut{};
    BOOL success = DeviceIoControl(
        Handle,
        IOCTL_CONDRV_ISSUE_USER_IO,
        &Descriptors,
        InSize,
        nullptr,
        0,
        &dwOut,
        nullptr);

    if (!success)
    {
        return E_UNEXPECTED;
    }

    return S_OK;
}

HRESULT
ConsoleCallServerWithBuffers(
    __in HANDLE Handle,
    __inout_bcount(sizeof(CONSOLE_MSG_HEADER) + ArgumentSize) PCONSOLE_MSG_HEADER Header,
    __in ULONG ApiNumber,
    __in ULONG ArgumentSize,
    __in_ecount_opt(InputCount) PCONSOLE_BUFFER InputBuffers,
    __in ULONG InputCount,
    __in_ecount_opt(OutputCount) PCONSOLE_BUFFER OutputBuffers,
    __in ULONG OutputCount)

/*++

Routine Description:

    This routine sends a request to the console server associated with the
    given console object. The connection handle will be used to carry the
    request.

Arguments:

    Handle - Supplies either a client handle (in which case ClientHandle
        should be NULL) or a connection handle (in which case ClientHandle
        may be non-NULL).

    ClientHandle - Supplies the console object whose server will receive the
        request, when Handle is a handle to a connection object.

    Header - Supplies the header of the contents of the request to be sent to
        the server. The actual payload must follow the header.

    ApiNumber - Supplies the number of request the be sent to the server.

    ArgumentSize - Supplies the size, in bytes, of the request payload that
        follows the header.

    InputBuffers - Optionally supplies an array of additional input buffers.

    InputCount - Supplies the number of elements in the input buffer array.

    OutputBuffers - Optionally supplies an array of additional output buffers.

    OutputCount - Supplies the number of elements in the output buffer array.

Return Value:

    HRESULT indicating the result of the call. The server determine the
    result.

--*/

{
    return ConsoleCallServerGeneric(ConsoleGetHandle(),
                                    Handle,
                                    Header,
                                    ApiNumber,
                                    ArgumentSize,
                                    InputBuffers,
                                    InputCount,
                                    OutputBuffers,
                                    OutputCount);
}

HRESULT
ConsoleCallServer(
    __in HANDLE Handle,
    __inout_bcount(sizeof(CONSOLE_MSG_HEADER) + ArgumentSize) PCONSOLE_MSG_HEADER Header,
    __in ULONG ApiNumber,
    __in ULONG ArgumentSize)

/*++

Routine Description:

    This routine sends a request to the console server associated with the
    given console object. The connection handle will be used to carry the
    request.

Arguments:

    Handle - Supplies the console object whose server will receive the
        request.

    Header - Supplies the header of the contents of the request to be sent to
        the server. The actual payload must follow the header.

    ApiNumber - Supplies the number of request the be sent to the server.

    ArgumentSize - Supplies the size, in bytes, of the request payload that
        follows the header.

Return Value:

    NTSTATUS indicating the result of the call. The server determine the
    result.

--*/

{
    return ConsoleCallServerGeneric(ConsoleGetHandle(),
                                    Handle,
                                    Header,
                                    ApiNumber,
                                    ArgumentSize,
                                    NULL,
                                    0,
                                    NULL,
                                    0);
}

ULONG TestL9()
{
    CONSOLE_MSG_L9 m;
    PCONSOLE_L9_TEST_API a = &m.u.TestApi;
    a->TestValue = 1024;
    ConsoleCallServer(ConsoleGetHandle(), &m, ConsoleTestApi, sizeof(*a));
    return a->ReplyValue;
}

#include <stdio.h>
int main() {
    auto rep = TestL9();
    fprintf(stderr, "Reply Val %8.08llx\n", (unsigned long long)rep);
    return 0;
}
