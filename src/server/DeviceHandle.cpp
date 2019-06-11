// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "DeviceHandle.h"

#include "WinNTControl.h"

#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020

/*++
Routine Description:
- This routine creates a handle to an input or output client of the given
  server. No control io is sent to the server as this request must be coming
  from the server itself.

Arguments:
- Handle - Receives a handle to the new client.
- ServerHandle - Supplies a handle to the server to which to attach the
                 newly created client.
- Name - Supplies the name of the client object.
- Inheritable - Supplies a flag indicating if the handle must be inheritable.

Return Value:
- NTSTATUS indicating if the client was successfully created.
--*/
[[nodiscard]] NTSTATUS
DeviceHandle::CreateClientHandle(
    _Out_ PHANDLE Handle,
    _In_ HANDLE ServerHandle,
    _In_ PCWSTR Name,
    _In_ BOOLEAN Inheritable)
{
    return _CreateHandle(Handle,
                         Name,
                         GENERIC_WRITE | GENERIC_READ | SYNCHRONIZE,
                         ServerHandle,
                         Inheritable,
                         FILE_SYNCHRONOUS_IO_NONALERT);
}

/*++
Routine Description:
- This routine creates a new server on the driver and returns a handle to it.

Arguments:
- Handle - Receives a handle to the new server.
- Inheritable - Supplies a flag indicating if the handle must be inheritable.

Return Value:
- NTSTATUS indicating if the console was successfully created.
--*/
[[nodiscard]] NTSTATUS
DeviceHandle::CreateServerHandle(
    _Out_ PHANDLE Handle,
    _In_ BOOLEAN Inheritable)
{
    return _CreateHandle(Handle,
                         L"\\Device\\ConDrv\\Server",
                         GENERIC_ALL,
                         NULL,
                         Inheritable,
                         0);
}

/*++
Routine Description:
- This routine opens a handle to the console driver.

Arguments:
- Handle - Receives the handle.
- DeviceName - Supplies the name to be used to open the console driver.
- DesiredAccess - Supplies the desired access mask.
- Parent - Optionally supplies the parent object.
- Inheritable - Supplies a boolean indicating if the new handle is to be made inheritable.
- OpenOptions - Supplies the open options to be passed to NtOpenFile. A common
                option for clients is FILE_SYNCHRONOUS_IO_NONALERT, to make the handle
                synchronous.

Return Value:
- NTSTATUS indicating if the handle was successfully created.
--*/
[[nodiscard]] NTSTATUS
DeviceHandle::_CreateHandle(
    _Out_ PHANDLE Handle,
    _In_ PCWSTR DeviceName,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ HANDLE Parent,
    _In_ BOOLEAN Inheritable,
    _In_ ULONG OpenOptions)

{
    ULONG Flags = OBJ_CASE_INSENSITIVE;

    if (Inheritable)
    {
        WI_SetFlag(Flags, OBJ_INHERIT);
    }

    UNICODE_STRING Name;
    Name.Buffer = (wchar_t*)DeviceName;
    Name.Length = (USHORT)(wcslen(DeviceName) * sizeof(wchar_t));
    Name.MaximumLength = Name.Length + sizeof(wchar_t);

    OBJECT_ATTRIBUTES ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes,
                               &Name,
                               Flags,
                               Parent,
                               NULL);

    IO_STATUS_BLOCK IoStatus;
    return WinNTControl::NtOpenFile(Handle,
                                    DesiredAccess,
                                    &ObjectAttributes,
                                    &IoStatus,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    OpenOptions);
}
