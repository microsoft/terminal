/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- device.h

Abstract:
- This header exists to reduce the differences in winconpty
  from the in-box windows source.
- Relies on components from Server to reach into ntdll for NtOpenFile
  to get at the NT namespace, which is required to open the console device.
--*/

#pragma once

#include "../server/DeviceHandle.h"

[[nodiscard]] static inline NTSTATUS CreateClientHandle(PHANDLE Handle, HANDLE ServerHandle, PCWSTR Name, BOOLEAN Inheritable)
{
    return DeviceHandle::CreateClientHandle(Handle, ServerHandle, Name, Inheritable);
}

[[nodiscard]] static inline NTSTATUS CreateServerHandle(PHANDLE Handle, BOOLEAN Inheritable)
{
    return DeviceHandle::CreateServerHandle(Handle, Inheritable);
}
