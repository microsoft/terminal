// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/utils.hpp"

using namespace Microsoft::Console;

#ifndef FILE_SYNCHRONOUS_IO_ALERT
#define FILE_SYNCHRONOUS_IO_ALERT 0x00000010
#endif
#ifndef FILE_SYNCHRONOUS_IO_NONALERT
#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020
#endif
#ifndef FILE_NON_DIRECTORY_FILE
#define FILE_NON_DIRECTORY_FILE 0x00000040
#endif

#define FileModeInformation (FILE_INFORMATION_CLASS)16

#define FILE_PIPE_BYTE_STREAM_TYPE 0x00000000
#define FILE_PIPE_BYTE_STREAM_MODE 0x00000000
#define FILE_PIPE_QUEUE_OPERATION 0x00000000

typedef struct _FILE_MODE_INFORMATION
{
    ULONG Mode;
} FILE_MODE_INFORMATION, *PFILE_MODE_INFORMATION;

extern "C" NTSTATUS NTAPI NtQueryInformationFile(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass);

extern "C" NTSTATUS NTAPI NtCreateNamedPipeFile(
    PHANDLE FileHandle,
    ULONG DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    ULONG NamedPipeType,
    ULONG ReadMode,
    ULONG CompletionMode,
    ULONG MaximumInstances,
    ULONG InboundQuota,
    ULONG OutboundQuota,
    PLARGE_INTEGER DefaultTimeout);

bool Utils::HandleWantsOverlappedIo(HANDLE handle) noexcept
{
    IO_STATUS_BLOCK statusBlock;
    FILE_MODE_INFORMATION modeInfo;
    const auto status = NtQueryInformationFile(handle, &statusBlock, &modeInfo, sizeof(modeInfo), FileModeInformation);
    return status == 0 && WI_AreAllFlagsClear(modeInfo.Mode, FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT);
}

// Creates an overlapped anonymous pipe. openMode should be either:
// * PIPE_ACCESS_INBOUND
// * PIPE_ACCESS_OUTBOUND
// * PIPE_ACCESS_DUPLEX
//
// I know, I know. MSDN infamously says
// > Asynchronous (overlapped) read and write operations are not supported by anonymous pipes.
// but that's a lie. The only reason they're not supported is because the Win32
// API doesn't have a parameter where you could pass FILE_FLAG_OVERLAPPED!
// So, we'll simply use the underlying NT APIs instead.
//
// Most code on the internet suggests creating named pipes with a random name,
// but usually conveniently forgets to mention that named pipes require strict ACLs.
// https://stackoverflow.com/q/60645 for instance contains a lot of poor advice.
// Anonymous pipes also cannot be discovered via NtQueryDirectoryFile inside the NPFS driver,
// whereas running a tool like Sysinternals' PipeList will return all those semi-named pipes.
//
// The code below contains comments to create unidirectional pipes.
Utils::Pipe Utils::CreateOverlappedPipe(DWORD openMode, DWORD bufferSize)
{
    LARGE_INTEGER timeout = { .QuadPart = -10'0000'0000 }; // 1 second
    UNICODE_STRING emptyPath{};
    IO_STATUS_BLOCK statusBlock;
    OBJECT_ATTRIBUTES objectAttributes{
        .Length = sizeof(OBJECT_ATTRIBUTES),
        .ObjectName = &emptyPath,
        .Attributes = OBJ_CASE_INSENSITIVE,
    };
    DWORD serverDesiredAccess = 0;
    DWORD clientDesiredAccess = 0;
    DWORD serverShareAccess = 0;
    DWORD clientShareAccess = 0;

    switch (openMode)
    {
    case PIPE_ACCESS_INBOUND:
        serverDesiredAccess = SYNCHRONIZE | GENERIC_READ | FILE_WRITE_ATTRIBUTES;
        clientDesiredAccess = SYNCHRONIZE | GENERIC_WRITE | FILE_READ_ATTRIBUTES;
        serverShareAccess = FILE_SHARE_WRITE;
        clientShareAccess = FILE_SHARE_READ;
        break;
    case PIPE_ACCESS_OUTBOUND:
        serverDesiredAccess = SYNCHRONIZE | GENERIC_WRITE | FILE_READ_ATTRIBUTES;
        clientDesiredAccess = SYNCHRONIZE | GENERIC_READ | FILE_WRITE_ATTRIBUTES;
        serverShareAccess = FILE_SHARE_READ;
        clientShareAccess = FILE_SHARE_WRITE;
        break;
    case PIPE_ACCESS_DUPLEX:
        serverDesiredAccess = SYNCHRONIZE | GENERIC_READ | GENERIC_WRITE;
        clientDesiredAccess = SYNCHRONIZE | GENERIC_READ | GENERIC_WRITE;
        serverShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
        clientShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;
        break;
    default:
        THROW_HR(E_UNEXPECTED);
    }

    // Cache a handle to the pipe driver.
    static const auto pipeDirectory = []() {
        UNICODE_STRING path = RTL_CONSTANT_STRING(L"\\Device\\NamedPipe\\");

        OBJECT_ATTRIBUTES objectAttributes{
            .Length = sizeof(OBJECT_ATTRIBUTES),
            .ObjectName = &path,
        };

        wil::unique_hfile dir;
        IO_STATUS_BLOCK statusBlock;
        THROW_IF_NTSTATUS_FAILED(NtCreateFile(
            /* FileHandle        */ dir.addressof(),
            /* DesiredAccess     */ SYNCHRONIZE | GENERIC_READ,
            /* ObjectAttributes  */ &objectAttributes,
            /* IoStatusBlock     */ &statusBlock,
            /* AllocationSize    */ nullptr,
            /* FileAttributes    */ 0,
            /* ShareAccess       */ FILE_SHARE_READ | FILE_SHARE_WRITE,
            /* CreateDisposition */ FILE_OPEN,
            /* CreateOptions     */ FILE_SYNCHRONOUS_IO_NONALERT,
            /* EaBuffer          */ nullptr,
            /* EaLength          */ 0));

        return dir;
    }();

    wil::unique_hfile server;
    objectAttributes.RootDirectory = pipeDirectory.get();
    THROW_IF_NTSTATUS_FAILED(NtCreateNamedPipeFile(
        /* FileHandle        */ server.addressof(),
        /* DesiredAccess     */ serverDesiredAccess,
        /* ObjectAttributes  */ &objectAttributes,
        /* IoStatusBlock     */ &statusBlock,
        /* ShareAccess       */ serverShareAccess,
        /* CreateDisposition */ FILE_CREATE,
        /* CreateOptions     */ 0, // would be FILE_SYNCHRONOUS_IO_NONALERT for a synchronous pipe
        /* NamedPipeType     */ FILE_PIPE_BYTE_STREAM_TYPE,
        /* ReadMode          */ FILE_PIPE_BYTE_STREAM_MODE,
        /* CompletionMode    */ FILE_PIPE_QUEUE_OPERATION, // would be FILE_PIPE_COMPLETE_OPERATION for PIPE_NOWAIT
        /* MaximumInstances  */ 1,
        /* InboundQuota      */ bufferSize,
        /* OutboundQuota     */ bufferSize,
        /* DefaultTimeout    */ &timeout));

    wil::unique_hfile client;
    objectAttributes.RootDirectory = server.get();
    THROW_IF_NTSTATUS_FAILED(NtCreateFile(
        /* FileHandle        */ client.addressof(),
        /* DesiredAccess     */ clientDesiredAccess,
        /* ObjectAttributes  */ &objectAttributes,
        /* IoStatusBlock     */ &statusBlock,
        /* AllocationSize    */ nullptr,
        /* FileAttributes    */ 0,
        /* ShareAccess       */ clientShareAccess,
        /* CreateDisposition */ FILE_OPEN,
        /* CreateOptions     */ FILE_NON_DIRECTORY_FILE, // would include FILE_SYNCHRONOUS_IO_NONALERT for a synchronous pipe
        /* EaBuffer          */ nullptr,
        /* EaLength          */ 0));

    return { std::move(server), std::move(client) };
}

// GetOverlappedResult() for professionals! Only for single-threaded use.
//
// GetOverlappedResult() used to have a neat optimization where it would only call WaitForSingleObject() if the state was STATUS_PENDING.
// That got removed in Windows 7, because people kept starting a read/write on one thread and called GetOverlappedResult() on another.
// When the OS sets Internal from STATUS_PENDING to 0 (= done) and then flags the hEvent, that doesn't happen atomically.
// This results in a race condition if a OVERLAPPED is used across threads.
HRESULT Utils::GetOverlappedResultSameThread(const OVERLAPPED* overlapped, DWORD* bytesTransferred) noexcept
{
    assert(overlapped != nullptr);
    assert(overlapped->hEvent != nullptr);
    assert(bytesTransferred != nullptr);

    __assume(overlapped != nullptr);
    __assume(overlapped->hEvent != nullptr);
    __assume(bytesTransferred != nullptr);

    if (overlapped->Internal == STATUS_PENDING)
    {
        if (WaitForSingleObjectEx(overlapped->hEvent, INFINITE, FALSE) != WAIT_OBJECT_0)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }

    // Assuming no multi-threading as per the function contract and
    // now that we ensured that hEvent is set (= read/write done),
    // we can safely read whatever want because nothing will set these concurrently.
    *bytesTransferred = gsl::narrow_cast<DWORD>(overlapped->InternalHigh);
    return HRESULT_FROM_NT(overlapped->Internal);
}
