/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WinNTControl.h

Abstract:
- This module helps wrap methods from NTDLL.dll to avoid needing Driver Kit headers in the project.

Author:
- Michael Niksa (MiNiksa) 14-Sept-2016

Revision History:
--*/

#pragma once

class WinNTControl
{
public:
    [[nodiscard]] static NTSTATUS NtOpenFile(_Out_ PHANDLE FileHandle,
                                             _In_ ACCESS_MASK DesiredAccess,
                                             _In_ POBJECT_ATTRIBUTES ObjectAttributes,
                                             _Out_ PIO_STATUS_BLOCK IoStatusBlock,
                                             _In_ ULONG ShareAccess,
                                             _In_ ULONG OpenOptions);

    ~WinNTControl();

private:
    WinNTControl();

    WinNTControl(WinNTControl const&) = delete;
    void operator=(WinNTControl const&) = delete;

    static WinNTControl& GetInstance();

    wil::unique_hmodule const _NtDllDll;

    typedef NTSTATUS(NTAPI* PfnNtOpenFile)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, ULONG, ULONG);
    PfnNtOpenFile const _NtOpenFile;
};
