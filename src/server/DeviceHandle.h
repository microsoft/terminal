/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DeviceHandle.h

Abstract:
- This module helps create client and server handles for interprocess communication via the driver.

Author:
- Michael Niksa (MiNiksa) 14-Sept-2016

Revision History:
--*/

#pragma once
namespace DeviceHandle
{
    [[nodiscard]] NTSTATUS
    CreateServerHandle(
        _Out_ PHANDLE Handle,
        _In_ BOOLEAN Inheritable);

    [[nodiscard]] NTSTATUS
    CreateClientHandle(
        _Out_ PHANDLE Handle,
        _In_ HANDLE ServerHandle,
        _In_ PCWSTR Name,
        _In_ BOOLEAN Inheritable);

    [[nodiscard]] NTSTATUS
    _CreateHandle(
        _Out_ PHANDLE Handle,
        _In_ PCWSTR DeviceName,
        _In_ ACCESS_MASK DesiredAccess,
        _In_opt_ HANDLE Parent,
        _In_ BOOLEAN Inheritable,
        _In_ ULONG OpenOptions);
};
